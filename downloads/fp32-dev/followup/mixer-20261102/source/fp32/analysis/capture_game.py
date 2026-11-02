"""Capture native guest screenshots with offsets into the real QEMU audio stream."""
from pathlib import Path
import argparse,json,os,subprocess,time,wave,struct
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('game');p.add_argument('--seconds',type=int,default=90);a=p.parse_args()
r=Path(__file__).resolve().parents[2];out=r/'fp32/captures'/a.name;out.mkdir(exist_ok=False)
stream=r/'fp32/games-ac97.wav';start=stream.stat().st_size
header=stream.read_bytes()[:44]
assert header[:4]==b'RIFF' and header[8:16]==b'WAVEfmt ' and header[36:40]==b'data'
fmt,channels,rate,byte_rate,align,bits=struct.unpack_from('<HHIIHH',header,20)
assert fmt==1 and bits%8==0
params=wave._wave_params(channels,bits//8,rate,0,'NONE','not compressed')
env=dict(os.environ,FP10_QMP_SOCKET=str(r/'fp32/nt31-game-qmp.sock'),FP10_SERIAL_SOCKET=str(r/'fp32/nt31-game-serial.sock'))
subprocess.run(['python3',str(r/'bin/serial-test.py'),'RUN',r'D:\FP32.EXE '+a.game],env=env,check=True)
t0=time.monotonic();rows=[]
while time.monotonic()-t0<a.seconds:
 index=len(rows);ppm=out/f'{index:03d}.ppm';png=ppm.with_suffix('.png')
 subprocess.run(['python3',str(r/'bin/qmp.py'),'shot',str(ppm)],env=env,check=True,stdout=subprocess.DEVNULL)
 Image.open(ppm).save(png);ppm.unlink()
 rows.append({'elapsed_seconds':round(time.monotonic()-t0,3),'audio_offset':stream.stat().st_size,'screenshot':png.name})
 (out/'timeline.json').write_text(json.dumps({'game':a.game,'audio_start':start,'frames':rows},indent=2)+'\n')
 time.sleep(2)
end=stream.stat().st_size;align=params.nchannels*params.sampwidth;start=44+((start-44)//align)*align;end=44+((end-44)//align)*align
with stream.open('rb') as f:f.seek(start);raw=f.read(end-start)
with wave.open(str(out/'guest-output.wav'),'wb') as w:w.setparams(params);w.writeframes(raw)
print(json.dumps({'capture':str(out),'screenshots':len(rows),'audio_bytes':len(raw),'sample_rate':params.framerate,'channels':params.nchannels}))
