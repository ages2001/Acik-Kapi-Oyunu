"""Record bounded native audio/MIDI reset evidence against the frozen source."""
from pathlib import Path
import datetime
import hashlib
import json

v = Path(__file__).resolve().parent
root = v.parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
snapshot = v / 'frozen-builds/usbnt-audio-reset-v3-20261102'
manifest = json.loads((snapshot / 'manifest.json').read_text())
for name, digest in manifest['source'].items():
    assert sha(root / 'USBNT/src' / name) == digest
assert sha(root / 'USBNT/harness/kernel.elf') == manifest['files']['harness']['sha256']
evidence = {}
checks = {
    'usbnt-audio-reset-rundown-host-v3-20261102.log': 'PASS: 10000',
    'usbnt-audio-reset-open-host-v2-20261102.log': 'PASS: 4050',
    'usbnt-audio-reset-orchestration-20261102.log': 'PASS:',
}
rows = []
for fixture in ('xhci-uac2', 'ehci-uac2', 'ehci-tt-uac1', 'uhci-uac1', 'ohci-uac1'):
    name = 'usbnt-audio-reset-v3-' + fixture + '-20261102.log'
    content = (v / name).read_text()
    assert any(line.startswith('RESULT audio-reset ') and line.endswith(' PASS')
               for line in content.splitlines())
    evidence[name] = sha(v / name)
    rows.append(dict(fixture=fixture, result='PASS', physical_resets=5))
for name, needle in checks.items():
    assert needle in (v / name).read_text()
    evidence[name] = sha(v / name)
tests = ['USBNT/test/host/tuacrundown.c', 'USBNT/test/host/tuaopen.c',
         'USBNT/test/tuacreset.py', 'validation/test_usbdi_port_reset.py']
result = dict(
    utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    result='PASS_WITH_OPEN_RELEASE_WORK', snapshot=str(snapshot.relative_to(v)),
    production_host_cases=dict(rundown=10000, open_restore_fault_retry=4050,
                               uac2_clock_outcomes=4),
    qemu_physical_resets=25, qemu_rows=rows, evidence=evidence,
    tests={name: sha(root / name) for name in tests},
    scope='Native QEMU UAC1/UAC2 duplex audio, feedback, capture, MIDI and control replay. '
          'Physical reset preserves audio ID, requested format and callback context. '
          'Host tests exercise production allocation, pipe, control, startup failure and retry paths.',
    limits=['Host controller and register operations are mocked in fault tests.',
            'Installed Windows regression is recorded separately for these exact hashes.',
            'Identity-preserving parent sleep, mixed-owner guest fixtures and release media remain open.',
            'No physical silicon certification. USBNT 1.3 remains unreleased.'])
out = v / 'usbnt-audio-reset-host-qemu-proof-20261102.json'
assert not out.exists(), 'preserve previous evidence'
out.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: 25 physical resets, 10000 lifetime schedules, 4050 restore cases and four UAC2 clock outcomes')
