"""Build test-only query/down/up controller IRPs for unplug-during-D3 tests.

Default operation is QUERY_POWER. Explicit /down and /up issue SET_POWER at
DISPATCH_LEVEL to the one USBNT Intel 24CD EHCI FDO. No ACPI sleep is claimed.
"""
from pathlib import Path
import datetime, hashlib, json, subprocess, sys
v=Path(__file__).resolve().parent
arch=sys.argv[1];assert arch in ('i386','amd64')
base=v/('usbnt-hc-power-single-'+arch+'-20261102')
out=v/('usbnt-sleep-probe-'+arch+'-20261102');out.mkdir(exist_ok=False)
s=(base/'hcpower_driver.c').read_text().replace('RltkHcPowerTest','RltkSleepTest').replace('HCPTEST','SLPTEST')
s=s.replace('#define SYSTEM_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_WRITE_ACCESS)', '#define SYSTEM_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_WRITE_ACCESS)\n#define RESUME_TEST CTL_CODE(FILE_DEVICE_UNKNOWN,0x802,METHOD_BUFFERED,FILE_WRITE_ACCESS)')
s=s.replace('POWER_STATE_TYPE type, ULONG state)', 'POWER_STATE_TYPE type, ULONG state, UCHAR minor)')
s=s.replace('sp->MinorFunction=IRP_MN_SET_POWER;', 'sp->MinorFunction=minor;')
s=s.replace('trace(type==SystemPowerState?', 'trace(minor==IRP_MN_QUERY_POWER?"SLPTEST Q3=":type==SystemPowerState?')
a=s.index('static NTSTATUS CyclePower(');b=s.index('\nstatic NTSTATUS NTAPI Dispatch',a)
s=s[:a]+'''static NTSTATUS CyclePower(PDEVICE_OBJECT target, int mode)
{
    return FunctionPower(target, DevicePowerState,
                         mode == 2 ? PowerDeviceD0 : PowerDeviceD3,
                         mode == 0 ? IRP_MN_QUERY_POWER : IRP_MN_SET_POWER);
}
''' + s[b:]
s=s.replace('sp->Parameters.DeviceIoControl.IoControlCode==SYSTEM_TEST))','sp->Parameters.DeviceIoControl.IoControlCode==SYSTEM_TEST||sp->Parameters.DeviceIoControl.IoControlCode==RESUME_TEST))')
s=s.replace('CyclePower(target,sp->Parameters.DeviceIoControl.IoControlCode==SYSTEM_TEST)', 'CyclePower(target,(sp->Parameters.DeviceIoControl.IoControlCode-RESET_TEST)/4)')
p=(base/'hcpower_probe.c').read_text().replace('RltkHcPowerTest','RltkSleepTest').replace('hcpower.sys','slptest.sys').replace('HC_POWER error=','SLEEP_POWER error=')
a=p.index('static int systemMode(');b=p.index('\nvoid Entry(',a)
p=p[:a]+'''static int operation(void){const char*s=GetCommandLineA();while(*s){if(s[0]==' '&&s[1]=='/'){if(lstrcmpA(s+2,"down")==0)return 1;if(lstrcmpA(s+2,"up")==0)return 2;}s++;}return 0;}
''' + p[b:]
p=p.replace('RESET_TEST+(systemMode()?4:0)','RESET_TEST+operation()*4')
(out/'slptest_driver.c').write_text(s);(out/'slptest_probe.c').write_text(p)
commands=json.loads((base/'manifest.json').read_text())['commands'];executed=[]
for command in commands:
    command=[c.replace(str(base),str(out)).replace('hcpower_driver.c','slptest_driver.c').replace('hcpower_probe.c','slptest_probe.c').replace('HCPOWER.SYS','SLPTEST.SYS').replace('HCPOWER.EXE','SLPTEST.EXE') for c in command]
    subprocess.run(command,check=True);executed.append(command)
(out/'manifest.json').write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),scope=__doc__,arch=arch,commands=executed,files={p.name:dict(size=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in out.iterdir()}),indent=2)+'\n')
print(out/'manifest.json')
