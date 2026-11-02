r"""Deploy small FP32 test files through a removable image into C:\FP32."""
from pathlib import Path
import subprocess
import sys
import time
import re
root = Path(__file__).resolve().parents[2]
image = root/'fp32/build'/('transfer-'+str(time.time_ns())+'.img')
def run(args):
    subprocess.run([str(x) for x in args], check=True, cwd=root)
names = sys.argv[1:]
assert names and all(Path(n).name == n for n in names)
assert all(re.fullmatch(r'[A-Za-z0-9_-]{1,8}(?:\.[A-Za-z0-9_]{1,3})?',n) for n in names), 'NT 3.1 floppy transfer requires 8.3 names'
run([root/'tools/usr/bin/mformat','-C','-f','1440','-i',image,'::'])
for name in names:
    run([root/'tools/usr/bin/mcopy','-i',image,root/'fp32/build'/name,'::/'])
run(['python3',root/'bin/qmp.py','hmp','eject floppy0'])
run(['python3',root/'bin/qmp.py','hmp','change floppy0 '+str(image)+' raw'])
for name in names:
    # An active software-rendered game can delay a floppy transfer beyond
    # the helper's normal 20-second deadline. Keep it bounded but allow 60s.
    run(['python3',root/'bin/serial-test.py','--timeout','60','COPY', 'A:' + chr(92) + name + '|C:' + chr(92) + 'FP32' + chr(92) + name])
