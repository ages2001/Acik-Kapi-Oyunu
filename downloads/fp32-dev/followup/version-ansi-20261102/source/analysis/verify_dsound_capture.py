"""Verify captured QEMU AC97 output, not synthesized replacement audio.

Run DSTEST in each guest after recording dsound-capture-start.json. The WAV
backend appends only while active; its length fields remain zero until exit.
"""
from pathlib import Path
import array
import hashlib
import json
import math
import struct
import wave

root = Path(__file__).resolve().parents[1]
results = {}
for osname, capture in json.loads((root/'fp32/reports/dsound-capture-start.json').read_text()).items():
    path = root/capture['file']
    raw = path.read_bytes()
    assert raw[:4] == b'RIFF' and raw[8:16] == b'WAVEfmt '
    assert struct.unpack_from('<HHI', raw, 20) == (1, 2, 44100)
    assert struct.unpack_from('<H', raw, 34)[0] == 16 and raw[36:40] == b'data'
    start = capture['offset']
    assert start >= 44 and (start-44) % 4 == 0
    data = raw[start:len(raw)-(len(raw)-44) % 4]
    samples = array.array('h', data)
    # Select the strongest half-second window entirely inside this test run.
    frames = 22050
    candidates = range(0, len(samples)-frames*2+1, 8820)
    offset = max(candidates, key=lambda i: sum(v*v for v in samples[i:i+frames*2]))
    segment = samples[offset:offset+frames*2]
    channels = [segment[0::2], segment[1::2]]

    def magnitude(channel, frequency):
        angle = 2*math.pi*frequency/44100
        coefficient = 2*math.cos(angle)
        a = b = 0.0
        for value in channel:
            c = value + coefficient*a - b
            b, a = a, c
        return math.sqrt(max(0, a*a+b*b-coefficient*a*b))/len(channel)

    spectra = [{str(hz): magnitude(channel, hz) for hz in (441, 630)} for channel in channels]
    rms = [math.sqrt(sum(v*v for v in channel)/len(channel)) for channel in channels]
    assert min(rms) > 100, (osname, rms)
    assert spectra[0]['441'] > 5*spectra[0]['630'], (osname, spectra)
    assert spectra[1]['630'] > 5*spectra[1]['441'], (osname, spectra)
    output = root/'fp32/reports'/('dsound-'+osname+'-captured.wav')
    with wave.open(str(output), 'wb') as wav:
        wav.setparams((2, 2, 44100, 0, 'NONE', 'not compressed'))
        wav.writeframes(segment.tobytes())
    results[osname] = {
        'capture': capture['file'], 'run_start_byte': start,
        'selected_byte': start+offset*2, 'sample_frames': frames,
        'sample_rate': 44100, 'channels': 2, 'rms': rms,
        'spectral_magnitude': spectra,
        'wav_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
        'scope': 'Original NT4 DirectSound foreground PCM probe via AUDIONT 1.1 AC97. Not Flash game audio, capture input, or GLOBALFOCUS support.'}
(root/'fp32/reports/dsound-capture-verification.json').write_text(json.dumps(results, indent=2)+'\n')
print(json.dumps(results, indent=2))
