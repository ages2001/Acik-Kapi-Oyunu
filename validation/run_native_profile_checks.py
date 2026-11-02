"""Fresh native-USB regression for profiles already installed in a test guest.
Each profile must pass actual PnP/ICMP after reinsert, then five endpoint
halts. Installation failures stop the suite; no setup dialogs are approved.
"""
from pathlib import Path
import re,subprocess,sys
guest,tag,*models=sys.argv[1:];here=Path(__file__).resolve().parent
assert re.fullmatch(r'[a-z0-9-]+',tag) and models
for model in models:
    assert model in ('8153','8156','8157','8159')
    name=tag+'-'+model
    subprocess.run([sys.executable,str(here/'native_usb_cycles.py'),guest,'usb-rtl'+model,'ehci.0','1','--tag='+name+'-switch'],check=True)
    subprocess.run([sys.executable,str(here/'native_stall_cycles.py'),guest,'5',name],check=True)
    subprocess.run([sys.executable,str(here/'capture_guest.py'),guest,guest+'-'+name+'-passed'],check=True)
    print('PASS: RTL'+model+' native reinsert and five stalls',flush=True)
