"""Collect actual six-NIC guest evidence; serialize the shared UDP endpoint."""
from pathlib import Path
import datetime,fcntl,json,re,socket,subprocess,sys
guest,token,exe=sys.argv[1:4];here=Path(__file__).resolve().parent
assert re.fullmatch(r'[A-Za-z0-9_-]+',token)
target=here/(guest+'-'+token+'.json');assert not target.exists()
with open('/tmp/rltklant-pci-report.lock','a') as lock:
    fcntl.flock(lock,fcntl.LOCK_EX)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1',18201));receiver.settimeout(60)
        subprocess.run([sys.executable,str(here/'type_guest.py'),guest,'type',' '+exe+' '+token+'\n'],check=True)
        data,peer=receiver.recvfrom(16384)
text=data.decode('ascii');print(text,flush=True)
target.write_text(json.dumps(dict(guest=guest,token=token,utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),peer=peer,report=text),indent=2)+'\n')
assert text.startswith('TOKEN '+token+'\r\n') and text.endswith('PASS\r\n'),text
names=[line[5:] for line in text.splitlines() if line.startswith('NAME ')]
expected=['Realtek RTL8127A 10GbE Controller']*2+[
    'Realtek RTL8126A 5GbE Controller','Realtek RTL8125A 2.5GbE Controller',
    'Realtek RTL8168b/8111b PCIe Gigabit Ethernet','Realtek RTL8169sc/8110sc Gigabit Ethernet']
if '--management-profiles' in sys.argv:
    expected=[n.replace('RTL8168b/8111b','RTL8168ep/8111ep').replace('RTL8125A','RTL8125BP') for n in expected]
    if '--management-8117' in sys.argv:
        expected=[n.replace('RTL8168ep/8111ep','RTL8168fp/8117') for n in expected]
assert sorted(names)==sorted(expected),names
assert text.count('PROBLEM 0 ')==6 and text.count(' REPLIES 4 PAYLOAD 1472')==6
