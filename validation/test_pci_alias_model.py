"""QEMU RTL8125 PCI alias contract; not independent silicon validation."""
from pathlib import Path
import datetime, hashlib, json, subprocess
from test_pcie_model import QEMU, test

v = Path(__file__).resolve().parent
output = v / 'qemu-pci-alias-model-20261102.json'
assert not output.exists()
result = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
          'qemu_sha256': hashlib.sha256(Path(QEMU).read_bytes()).hexdigest(),
          'result': 'RUNNING', 'profiles': [], 'rejected': [],
          'scope': 'PCI aliases share the modeled RTL8125A register contract; no physical or distinct silicon-revision claim'}
try:
    for alias in (0x3000, 0x5000, 0x0e10):
        for mmio in (False, True):
            model = 'rtl8125,pci-id-alias=' + hex(alias) + (',io-bar=off' if mmio else '')
            test(model, alias, 0x400)
            result['profiles'].append({'alias': hex(alias), 'mmio_only': mmio,
                                       'checks': 'PCIe identity, PHY/OCP, XID, TX/RX/FCS, DMA bounds, carrier, reset'})
    for model in ('rtl8125,pci-id-alias=0x1234', 'rtl8127,pci-id-alias=0x3000',
                  'rtl8169,variant=2,pci-id-alias=0x3000'):
        process = subprocess.run([QEMU, '-machine', 'q35', '-nodefaults', '-display',
                                  'none', '-S', '-device', model], capture_output=True, text=True, timeout=10)
        assert process.returncode != 0 and 'pci-id-alias requires' in process.stderr, process
        result['rejected'].append(model)
    result['result'] = 'PASS'
except Exception as error:
    result['result'] = 'FAIL'; result['error'] = repr(error)
    raise
finally:
    output.write_text(json.dumps(result, indent=2) + '\n')
