"""Collect final native mixer/timer reports and verify tested binary aliases."""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess

root = Path(__file__).resolve().parents[1]
results = {}
for osname, socket in [('nt31', 'fp32/nt31-guard-query-serial.sock'), ('nt351', 'nt351/upload-serial.sock')]:
    for log, report, count in [
        ('MIXER.LOG', 'mixer-'+osname+'-final', 40),
        ('MIXER10.LOG', 'mixer-fp10-'+osname+'-final', 40),
        ('MMTIMER.LOG', 'multimedia-timer-'+osname+'-final', 18 if osname == 'nt31' else 17),
        ('DSOUND.LOG', 'dsound-'+osname+'-final', 16),
    ]:
        target = root/'fp32/reports'/(report+'.txt')
        subprocess.run(['python3', str(root/'fp32/analysis/read_guest_file.py'),
                        'C:\\FP32\\'+log, str(target)],
                       env=dict(os.environ, FP10_SERIAL_SOCKET=str(root/socket)),
                       check=True, capture_output=True)
        text = target.read_text()
        assert int(re.search(r'Checks (\d+)', text)[1]) == count, target
        assert int(re.search(r'Failures (\d+)', text)[1]) == 0, target
        results[report] = {'checks': count, 'failures': 0,
                           'sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
for alias, production in [('fp32/build/M32M.DLL', 'fp32/build/F32M.DLL'),
                          ('fp32/build/M10M.DLL', 'build/fp10-dynamic/FPM.DLL')]:
    assert (root/alias).read_bytes() == (root/production).read_bytes(), alias
for name in ['FPMM.DLL', 'FPNTM.DLL', 'FPMK.DLL']:
    assert (root/'fp32/build'/name).read_bytes() == (root/'build/fp10-dynamic'/name).read_bytes()
results['scope'] = 'Native NT3.1/3.51 UP classic VGA with AUDIONT 1.1 AC97. Component tests, not full-game or all-OS validation. DirectSound remains experimental and excluded from default imports.'
(root/'fp32/reports/mixer-suite-summary.json').write_text(json.dumps(results, indent=2)+'\n')
print(json.dumps(results, indent=2))
