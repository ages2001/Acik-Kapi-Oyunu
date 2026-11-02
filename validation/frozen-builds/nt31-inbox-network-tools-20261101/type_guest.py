import socket,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'USBNT/test'))
from ntvm import NtVm
v=NtVm.__new__(NtVm);v.qmp=socket.socket(socket.AF_UNIX);v.qmp.settimeout(15)
v.qmp.connect('/workspace/rltklant-vm/run/'+sys.argv[1]+'/qmp.sock');v.qbuf=b''
v._read();v.cmd('qmp_capabilities')
if sys.argv[2]=='key':v.key(sys.argv[3])
else:v.type(sys.argv[3])
v.qmp.close()
time.sleep(0.6)  # allow the guest UI to dispatch the completed input
