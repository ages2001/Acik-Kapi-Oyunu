"""Compile and preserve standalone MAC source/fault checks; no guest claims."""
from pathlib import Path
import argparse, datetime, hashlib, json, os, subprocess, tempfile

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('label')
p.add_argument('--hardware',action='store_true',help='Also test hardware preparation and DASH acquisition/release')
a = p.parse_args()
v = Path(__file__).resolve().parent
root = v.parent
out = v / a.label
assert out.parent == v and not out.exists(), 'preserve previous evidence'
out.mkdir()
commands = []
def run(args):
    result = subprocess.run(args, cwd=root, text=True, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT)
    commands.append({'command': args, 'exit_code': result.returncode,
                     'output': result.stdout})
    (out / 'commands.json').write_text(json.dumps(commands, indent=2) + '\n')
    print(result.stdout, end='', flush=True)
    result.check_returncode()

flags = ['-std=gnu99', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function']
run(['python3', 'RLTKLANT/build/make-pci-mac.py', '--check'])
with tempfile.TemporaryDirectory(prefix='rltk-mac-check-') as tmp:
    binary = str(Path(tmp) / 'test')
    run(['gcc', *flags, '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
         'validation/test_mac_hardware.c' if a.hardware else 'validation/test_macseq.c', 'RLTKLANT/src/macseq.c',
         'RLTKLANT/src/fwbus.c', 'RLTKLANT/src/fwvm.c',
         'RLTKLANT/src/fwimages.c', '-o', binary])
    run([binary])
    for arch in ('i686', 'x86_64'):
        run([arch + '-w64-mingw32-gcc', *flags, '-ffreestanding', '-c',
             'RLTKLANT/src/macseq.c', '-o', str(Path(tmp) / (arch + '.o'))])
paths = ['validation/test_macseq.c', 'validation/run_macseq_checks.py',
         'RLTKLANT/build/make-pci-mac.py']
if a.hardware:paths.append('validation/test_mac_hardware.c')
paths += ['RLTKLANT/src/' + n for n in ('macseq.c', 'macseq.h', 'fwbus.c',
           'fwbus.h', 'fwvm.c', 'fwvm.h', 'fwimages.c', 'fwimages.h')]
paths += ['RLTKLANT/third_party/r8169/' + n for n in ('r8169_main.c',
           'mac-sequences.inc', 'mac-constants.h', 'MAC-PROVENANCE.json', 'versions.h')]
manifest = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'result': 'PASS_STANDALONE_TRANSPORT',
            'sources': {s: hashlib.sha256((root/s).read_bytes()).hexdigest() for s in paths},
            'limits': ['Standalone transport evidence only; Windows lifecycle integration has separate proof. The new hardware/DASH API is not integrated yet.',
                       'Synthetic register readiness does not execute PHY/MCU firmware or validate silicon.',
                       'Full MAC power/startup, resource mapping and lifecycle ordering remain separate.']}
(out/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
print(out/'manifest.json')
