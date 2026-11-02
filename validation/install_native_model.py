"""Switch a USB protocol model and start explicit NT5 driver installation.
The caller inspects/handles the OS installation dialog and verifies the result.
"""
import json,subprocess,sys,time
from pathlib import Path
name,model=sys.argv[1:];v=Path(__file__).resolve().parent
assert model in ['usb-rtl8153','usb-rtl8156','usb-rtl8157','usb-rtl8159']
def call(script,*args):subprocess.run([sys.executable,str(v/script),name,*args],check=True)
call('qmp.py','device_del',json.dumps({'id':'usbnic'}));time.sleep(3)
if name.startswith('win2000'):call('type_guest.py','key','ret')
call('qmp.py','device_add',json.dumps(dict(driver=model,id='usbnic',bus='ehci.0',port='1',netdev='n0',mac='52:54:00:ab:cd:ef')));time.sleep(3)
# USB bcdDevice is not the silicon version read from PLA registers. There
# is exactly one model NIC here; update its real vendor/product hardware ID.
call('type_guest.py','type','c:\\rlttest\\devprobe update c:\\rlttest\\netrtlk.inf "USB\\VID_0BDA&PID_'+model[-4:]+'"');call('type_guest.py','key','ret')
