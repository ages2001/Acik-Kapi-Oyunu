"""Collect actual legacy guest payload verification on selected fixture paths."""
from pathlib import Path
import datetime, fcntl, json, re, socket, subprocess, sys

guest, token, mask = sys.argv[1:4]
here = Path(__file__).resolve().parent
assert re.fullmatch(r'[A-Za-z0-9_-]{1,48}', token)
mask = int(mask); assert 1 <= mask <= 7
output = here / (guest + '-' + token + '.json')
assert not output.exists(), 'preserve prior evidence'
with open('/tmp/rltklant-legacy-report.lock', 'a') as lock:
    fcntl.flock(lock, fcntl.LOCK_EX)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1', 18202)); receiver.settimeout(50)
        subprocess.run([sys.executable, str(here / 'type_guest.py'), guest, 'type',
                        ' C:\\RLTTEST\\LNET.EXE ' + token + ' ' + str(mask) + '\n'], check=True)
        data, peer = receiver.recvfrom(4096)
text = data.decode('ascii')
output.write_text(json.dumps({'guest': guest, 'token': token, 'mask': mask,
    'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'peer': peer,
    'report': text}, indent=2) + '\n')
print(text, flush=True)
assert text.startswith('TOKEN ' + token + '\r\nMASK ' + str(mask) + '\r\n'), text
assert text.endswith('PASS\r\n'), text
expected = ['10.0.2.2', '10.0.3.2', '10.0.4.2']
paths = re.findall(r'^PATH (\S+) REPLIES (\d+) PAYLOAD (\d+)\r?$', text, re.M)
assert paths == [(p, '4', '1472') for i, p in enumerate(expected) if mask & (1 << i)], paths
assert 'TOTAL %d EXPECTED %d\r\n' % (4 * mask.bit_count(), 4 * mask.bit_count()) in text
