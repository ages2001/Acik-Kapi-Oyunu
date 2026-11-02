"""Build a test-only USBNT EHCI FDO D3/D0 and S3/S0 IRP injector.

The S pairs test system-to-device IRP mapping, not actual ACPI machine sleep.
Only the single QEMU Intel 24CD EHCI controller is selected. The kernel helper
requires the attached FDO's driver to be USBNT and retains it across calls.
"""
from pathlib import Path
import sys,json,subprocess,hashlib,datetime
here=Path(__file__).resolve().parent
out=Path(sys.argv[1]).resolve();arch=sys.argv[2] if len(sys.argv)>2 else 'i386'
base=here/('usbnt-power-probe-20261102' if arch=='i386' else 'usbnt-power-probe-amd64-fixed-20261102')
out.mkdir(exist_ok=False)
s=(base/'upower_driver.c').read_text().replace('RltkPowerTest','RltkHcPowerTest').replace('POWERTEST','HCPTEST')
s=s.replace('target=file->DeviceObject;', 'target=IoGetAttachedDeviceReference(file->DeviceObject);')
s=s.replace('                ObDereferenceObject(file);','                ObDereferenceObject(target);\n                ObDereferenceObject(file);')
s=s.replace('DEVICE_POWER_STATE state)','POWER_STATE_TYPE type, ULONG state)')
s=s.replace('sp->Parameters.Power.Type=DevicePowerState;', 'sp->Parameters.Power.Type=type;')
s=s.replace('trace(state==PowerDeviceD3?"HCPTEST D3=":"HCPTEST D0=",done.Status);', 'trace(type==SystemPowerState?(state==PowerSystemSleeping3?"HCPTEST S3=":"HCPTEST S0="):(state==PowerDeviceD3?"HCPTEST D3=":"HCPTEST D0="),done.Status);')
s=s.replace('LARGE_INTEGER delay;int i;', 'LARGE_INTEGER delay;int i,mode;')
s=s.replace('    for(i=0;i<3;i++){','    for(mode=0;mode<2;mode++)for(i=0;i<3;i++){')
s=s.replace('FunctionPower(target,PowerDeviceD3)', 'FunctionPower(target,mode?SystemPowerState:DevicePowerState,mode?PowerSystemSleeping3:PowerDeviceD3)')
s=s.replace('FunctionPower(target,PowerDeviceD0)', 'FunctionPower(target,mode?SystemPowerState:DevicePowerState,mode?PowerSystemWorking:PowerDeviceD0)')
s=s.replace('        if(!NT_SUCCESS(resume))return resume;','        if(!NT_SUCCESS(resume))return resume;\n        delay.QuadPart=-30000000;KeDelayExecutionThread(KernelMode,FALSE,&delay);')
s=s.replace('Test-only USBNT function-power helper. Send D3/D0 to a named PDO.', 'Test-only USBNT EHCI controller power IRP helper; no actual ACPI sleep.')
p=(base/'upower_probe.c').read_text().replace('RltkPowerTest','RltkHcPowerTest').replace('upower.sys','hcpower.sys').replace('FUNCTION_POWER error=', 'HC_POWER error=')
p=p.replace('VID_0BDA',r'PCI\\VEN_8086&DEV_24CD')
(out/'hcpower_driver.c').write_text(s);(out/'hcpower_probe.c').write_text(p)
commands=json.loads((base/'manifest.json').read_text())['commands']
for command in commands:
 command=[c.replace(str(base),str(out)).replace('upower_driver.c','hcpower_driver.c').replace('upower_probe.c','hcpower_probe.c').replace('UPOWER.SYS','HCPOWER.SYS').replace('UPOWER.EXE','HCPOWER.EXE') for c in command]
 subprocess.run(command,check=True)
manifest={'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':__doc__,'arch':arch,'files':{p.name:{'size':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in out.iterdir()}}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n');print(out/'manifest.json')
