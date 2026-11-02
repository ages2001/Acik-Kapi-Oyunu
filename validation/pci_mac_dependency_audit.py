"""Inventory source requirements for the unported PCI MAC startup boundary.

Identity or PHY test coverage must never be substituted for these MAC steps.
This reads the pinned upstream source; it does not claim runtime support.
"""
from pathlib import Path
import argparse,hashlib,json,re
p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
raw=a.source.read_bytes();assert hashlib.sha256(raw).hexdigest()=='cb918734e19369d22fd0e65657c0183518f0ba59b57745a74f6a5bbcf56f37cc'
s=raw.decode();clean=re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"',lambda m:re.sub(r'[^\n]',' ',m[0]),s,flags=re.S);functions={}
for m in re.finditer(r'^static\s+[\w\s*]+?\b(\w+)\s*\([^;{}]*\)\s*\{',clean,re.M):
 depth=1;end=m.end()
 while depth:depth+=(clean[end]=='{')-(clean[end]=='}');end+=1
 functions[m[1]]=(m.start(),m.end()-1,end)
roots=['rtl_hw_config','rtl_hw_start_8125','rtl_hw_start_8168','rtl_hw_start_8169']
boundary={'rtl_eri_read','rtl_eri_write','r8168_mac_ocp_read','r8168_mac_ocp_write','rtl_ephy_read','rtl_ephy_write','r8169_apply_firmware','rtl_loop_wait_low','rtl_loop_wait_high','rtl_csi_access_enable','rtl8168ep_stop_cmac','rtl_set_aspm_entry_latency'}
selected=set();external=set()
def visit(name):
 if name in selected or name in boundary or name not in functions:return
 selected.add(name);start,body,end=functions[name]
 # Include function-pointer table entries as well as ordinary calls.
 for ident in set(re.findall(r'\b\w+\b',clean[body:end])):
  if ident in functions:visit(ident)
 for called in re.findall(r'\b(\w+)\s*\(',clean[body:end]):
  if called not in functions:external.add(called)
for root in roots:visit(root)
rows=[]
for name in sorted(selected,key=lambda n:functions[n][0]):
 start,body,end=functions[name];rows.append({'name':name,'line':s.count('\n',0,start)+1,'sha256':hashlib.sha256(s[start:end].encode()).hexdigest()})
result={'result':'IMPLEMENTATION_GAP_INVENTORY','source_sha256':hashlib.sha256(raw).hexdigest(),'roots':roots,'functions':rows,'explicit_boundary':sorted(boundary),'external_calls':sorted(external-{'if','for','while','switch','return','sizeof'}),'required_integration':['Per-revision MAC/EPHY/ERI/CSI prerequisites before enabling DMA, with bounded error propagation','RTL8125+ register windows include 0x1880; current NT3.1 mapping is only 0x100 and current QEMU BAR is 0x1000','PCI capability/extended configuration access and source-based ASPM policy','RTL8117 separate MAC-only firmware step and CMAC stop handshakes','Stop/resume/reset ordering and separate SFP/SerDes paths'],'limits':['This inventory is not a port, test pass, or proof of physical silicon behavior.','Selected roots describe the MAC configuration subtree; full driver initialization and lifecycle require additional upstream functions.']}
assert not a.output.exists();a.output.write_text(json.dumps(result,indent=2)+'\n');print('Inventoried',len(rows),'pinned MAC functions; implementation remains open')
