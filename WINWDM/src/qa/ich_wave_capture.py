"""Check that QEMU captured the two channel tones played by ICHWAVE.EXE."""
import argparse
from array import array
import math
import statistics
import sys
import wave


def frequency(channel, rate):
    threshold = max(map(abs, channel)) / 4
    armed = False
    rises = []
    for index, sample in enumerate(channel):
        if sample < -threshold:
            armed = True
        elif armed and sample > threshold:
            rises.append(index)
            armed = False
    if len(rises) < 2:
        return 0
    return (len(rises) - 1) * rate / (rises[-1] - rises[0])


def measure(path):
    with wave.open(str(path), "rb") as stream:
        if stream.getnchannels() != 2 or stream.getsampwidth() != 2 or stream.getcomptype() != "NONE":
            raise ValueError("expected stereo 16-bit PCM")
        rate = stream.getframerate()
        samples = array("h")
        samples.frombytes(stream.readframes(stream.getnframes()))
        if sys.byteorder != "little":
            samples.byteswap()
    quarter = rate // 4
    if not quarter or rate < 3200:
        raise ValueError("sample rate is too low for the test tones")
    matches = []
    # Require three successive quarter-second windows with the expected tones.
    # Short endpoint transients and silence before playback are allowed.
    for offset in range(0, len(samples) // 2 - quarter + 1, quarter):
        channels = [samples[2*offset+i:2*(offset+quarter):2] for i in (0, 1)]
        values = [frequency(channel, rate) for channel in channels]
        if (all(math.sqrt(sum(x*x for x in c) / len(c)) >= 64 for c in channels)
                and all(abs(actual - target) <= 12 for actual, target in zip(values, (997, 1511)))):
            matches.append(values)
            if len(matches) == 3:
                return rate, tuple(statistics.median(column) for column in zip(*matches))
        else:
            matches.clear()
    raise ValueError("the recording lacks 750 ms of the expected left and right channel tones")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("wav")
    args = parser.parse_args()
    rate, (left, right) = measure(args.wav)
    print(f"Stereo PCM at {rate} Hz. Measured left {left:.2f} Hz and right {right:.2f} Hz.")
