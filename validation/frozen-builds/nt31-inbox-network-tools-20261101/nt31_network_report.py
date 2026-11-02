"""NT3.1 inbox ping receipt plus independent complete wire-payload matching.

The NT3.1 ICMP DLL predates IcmpSendEcho. Each path gets a freshly cleared
console, fresh capture offset and screenshot/OCR of the inbox ping summary.
This validates guest receipt and full wire payload, not a custom guest-side
payload comparator. No other test may own this guest during the probe.
"""
from pathlib import Path
import collections, datetime, hashlib, json, re, struct, subprocess, sys, time

guest, token, mask = sys.argv[1:4]
assert guest.startswith('nt31-') and re.fullmatch(r'[A-Za-z0-9_-]{1,48}', token)
mask = int(mask); assert mask in (1, 2, 3)
here = Path(__file__).resolve().parent
base = Path('/workspace/rltklant-vm/run') / guest
output = here / (guest + '-' + token + '.json')
assert not output.exists()
result = {'guest': guest, 'token': token, 'mask': mask, 'result': 'RUNNING',
          'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(), 'paths': [],
          'scope': 'NT3.1 inbox ping confirms guest receipt; host matches all 1472 wire payload bytes and ICMP identifier/sequence. This is not the LNET guest-side payload comparator.'}

def checksum(data):
    if len(data) & 1:
        data += b'\0'
    total = sum(struct.unpack('>%dH' % (len(data)//2), data))
    while total >> 16:
        total = (total & 65535) + (total >> 16)
    return total == 65535

def counts(path, start, address):
    data = path.read_bytes(); at = start
    requests, replies = collections.Counter(), collections.Counter()
    while at + 16 <= len(data):
        _, _, length, original = struct.unpack_from('<IIII', data, at)
        if at + 16 + length > len(data):
            break
        frame = data[at+16:at+16+length]; at += 16+length
        if len(frame) < 42 or frame[12:14] != b'\x08\x00' or frame[23] != 1:
            continue
        ihl = (frame[14] & 15) * 4
        if frame[14] >> 4 != 4 or ihl < 20:
            continue
        total, fragments = struct.unpack_from('>HH', frame, 16)[0], struct.unpack_from('>H', frame, 20)[0]
        if total != ihl+8+1472 or len(frame) < 14+total or fragments & 0x3fff:
            continue
        ip = frame[14:14+total]; packet = ip[ihl:]
        assert checksum(ip[:ihl]) and checksum(packet), 'invalid captured IP/ICMP checksum'
        if packet[0] == 8 and packet[1] == 0 and ip[16:20] == address:
            requests[(ip[12:16], ip[16:20], packet[4:])] += 1
        elif packet[0] == 0 and packet[1] == 0 and ip[12:16] == address:
            replies[(ip[16:20], ip[12:16], packet[4:])] += 1
    return sum(requests.values()), sum((requests & replies).values()), at, data

try:
    for bit, name, destination in [(1, 'usb.pcap', '10.0.2.2'), (2, 'pci.pcap', '10.0.3.2')]:
        if not mask & bit:
            continue
        path = base / name; start = path.stat().st_size
        command = 'cls\nping -n 4 -l 1472 ' + destination + '\n'
        subprocess.run([sys.executable, str(here/'type_guest.py'), guest, 'type', command], check=True)
        deadline = time.monotonic() + 30; address = bytes(map(int, destination.split('.')))
        while True:
            requests, matched, end, data = counts(path, start, address)
            if requests >= 4 and matched >= 4:
                break
            if time.monotonic() >= deadline:
                raise AssertionError((destination, requests, matched))
            time.sleep(.1)
        assert requests == matched == 4
        time.sleep(1)  # allow the inbox utility to print its final receipt summary
        stem = guest + '-' + token + '-' + name.split('.')[0]
        subprocess.run([sys.executable, str(here/'capture_guest.py'), guest, stem], check=True)
        ocr = subprocess.check_output(['tesseract', str(here/(stem+'.png')), 'stdout', '--psm', '6'], text=True)
        (here/(stem+'.ocr.txt')).write_text(ocr)
        pattern = r'10[.\-]0[.\-]' + destination.split('.')[2] + r'[.\-]2\s+replied to all 4 of the 4 pings'
        assert re.search(pattern, ocr, re.I), 'inbox ping receipt summary was not verified: ' + ocr
        result['paths'].append({'destination': destination, 'requests': requests, 'matched_replies': matched,
            'payload_bytes': 1472, 'pcap': name, 'start_offset': start, 'end_offset': end,
            'capture_prefix_sha256': hashlib.sha256(data[:end]).hexdigest(),
            'screenshot': stem+'.png', 'ocr': stem+'.ocr.txt',
            'screenshot_sha256': hashlib.sha256((here/(stem+'.png')).read_bytes()).hexdigest()})
        output.write_text(json.dumps(result, indent=2)+'\n')
    result['result'] = 'PASS'
    result['full_payload_replies'] = sum(p['matched_replies'] for p in result['paths'])
except Exception as error:
    result['result'] = 'FAIL'; result['error'] = repr(error)
    raise
finally:
    result['finished_utc'] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    output.write_text(json.dumps(result, indent=2)+'\n')
print('PASS NT3.1 inbox receipt and wire payload:', result['full_payload_replies'], flush=True)
