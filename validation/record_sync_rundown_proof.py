"""Record bounded synchronous-transfer lifetime and native-QEMU regressions."""
from pathlib import Path
import datetime
import hashlib
import json

v = Path(__file__).resolve().parent
root = v.parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
snapshot = v / 'frozen-builds/usbnt-sync-rundown-v2-20261102'
manifest = json.loads((snapshot / 'manifest.json').read_text())
for name, digest in manifest['source'].items():
    assert sha(root / 'USBNT/src' / name) == digest, name
assert sha(root / 'USBNT/harness/kernel.elf') == manifest['files']['harness']['sha256']
checks = {
    'usbnt-sync-rundown-host-v1-20261102.log': 'PASS: 200 delayed callback/event/timeout schedules and 100 submission failures',
    'usbnt-sync-rundown-v2-audio-20261102.log': 'RESULT audio-reset xhci_uac2_hs PASS',
    'usbnt-sync-rundown-v2-video-20261102.log': 'RESULT uvc-reset ehci_bulk PASS',
    'usbnt-sync-rundown-v2-cardreader-20261102.log': 'RESULT rtsx-reset usb-ehci 0x0140 PASS',
}
for fixture in ('xhci-uas', 'ehci-bot', 'uhci-bot', 'ohci-bot'):
    name = 'usbnt-sync-rundown-v2-' + fixture + '-20261102.log'
    content = (v / name).read_text()
    assert any(line.startswith('RESULT ') and 'PASS' in line for line in content.splitlines()), name
    checks[name] = 'RESULT '
for name, needle in checks.items():
    assert needle in (v / name).read_text(), name
before = v / 'usbnt-sync-rundown-before-20261102.log'
assert 'FAIL: synchronous caller returned while callback still owns event/transfer' in before.read_text()
result = dict(
    utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    result='PASS_WITH_OPEN_WINDOWS_AND_RELEASE_WORK',
    snapshot=str(snapshot.relative_to(v)),
    files=manifest['files'],
    host_cases=dict(delayed_callback_event_timeout=200, submission_failure=100),
    native_physical_resets=dict(storage=20, audio_midi=5, video=5, card_reader=5),
    evidence={name: sha(v / name) for name in checks},
    old_source_failure=dict(file=before.name, sha256=sha(before)),
    tests={name: sha(root / name) for name in (
        'USBNT/test/host/tsyncrundown.py', 'USBNT/test/tmscreset.py',
        'USBNT/test/tuacreset.py', 'USBNT/test/tuvcreset.py', 'USBNT/test/trtsxreset.py')},
    scope='The synchronous caller retains its event, transfer and DMA owner until the collected '
          'completion finishes accessing them, including after cancellation or I/O timeout. '
          'The production-source host test detects early return in the prior source. '
          'Native QEMU regressions retain storage data, stream settings and sibling input.',
    limits=['Installed Windows evidence for these exact hashes is separate.',
            'The prior NT3.51 RTL8159 attachment latency failure is not proven fixed.',
            'Identity-preserving parent sleep, mixed-owner guest fixtures and release media remain open.',
            'No physical silicon certification. USBNT 1.3 remains unreleased.'])
out = v / 'usbnt-sync-rundown-host-qemu-proof-20261102.json'
assert not out.exists(), 'preserve previous evidence'
out.write_text(json.dumps(result, indent=2) + '\n')
print('PASS: synchronous lifetime host regression and 35 native QEMU resets')
