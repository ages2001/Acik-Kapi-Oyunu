"""Build an explicit USBNT-only PDO power test, separate from RESET_PORT.

The helper sends D3/D0 at DISPATCH_LEVEL while retaining the PDO file object.
It tests USBNT function queue rundown, not NDIS system sleep or port suspend.
"""
from pathlib import Path
import datetime
import hashlib
import json
import os
import subprocess
import sys

here = Path(__file__).resolve().parent
out = Path(sys.argv[1]).resolve()
arch = sys.argv[2] if len(sys.argv) > 2 else 'i386'
assert arch in ('i386', 'amd64')
out.mkdir(exist_ok=False)
source = (here/'guest_usb_reset_driver.c').read_text()
power = r'''
typedef struct {KEVENT Event;NTSTATUS Status;} POWER_DONE;
static NTSTATUS NTAPI PowerDone(PDEVICE_OBJECT dev,PIRP irp,PVOID arg)
{
    POWER_DONE *done=(POWER_DONE*)arg;(void)dev;
    done->Status=irp->IoStatus.Status;
    KeSetEvent(&done->Event,IO_NO_INCREMENT,FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}
static NTSTATUS FunctionPower(PDEVICE_OBJECT target,DEVICE_POWER_STATE state)
{
    POWER_DONE done;PIRP request;PIO_STACK_LOCATION sp;KIRQL old;
    request=IoAllocateIrp(target->StackSize,FALSE);
    if(!request)return STATUS_INSUFFICIENT_RESOURCES;
    KeInitializeEvent(&done.Event,NotificationEvent,FALSE);
    done.Status=STATUS_NOT_SUPPORTED;request->IoStatus.Status=STATUS_NOT_SUPPORTED;
    sp=IoGetNextIrpStackLocation(request);
    sp->MajorFunction=IRP_MJ_POWER;sp->MinorFunction=IRP_MN_SET_POWER;
    sp->Parameters.Power.Type=DevicePowerState;
    sp->Parameters.Power.State.DeviceState=state;
    IoSetCompletionRoutine(request,PowerDone,&done,TRUE,TRUE,TRUE);
    /* Exercise the deferred path, not only the caller's passive fast path. */
    KeRaiseIrql(DISPATCH_LEVEL,&old);
    PoCallDriver(target,request);
    KeLowerIrql(old);
    KeWaitForSingleObject(&done.Event,Executive,KernelMode,FALSE,NULL);
    IoFreeIrp(request);
    trace(state==PowerDeviceD3?"POWERTEST D3=":"POWERTEST D0=",done.Status);
    return done.Status;
}
static NTSTATUS CyclePower(PDEVICE_OBJECT target)
{
    NTSTATUS status=STATUS_SUCCESS,resume;LARGE_INTEGER delay;int i;
    for(i=0;i<3;i++){
        status=FunctionPower(target,PowerDeviceD3);
        delay.QuadPart=-2000000; /* 200 ms; keep RX requests active around each transition. */
        KeDelayExecutionThread(KernelMode,FALSE,&delay);
        resume=FunctionPower(target,PowerDeviceD0); /* attempt D0 even if D3 failed */
        if(!NT_SUCCESS(status))return status;
        if(!NT_SUCCESS(resume))return resume;
    }
    return STATUS_SUCCESS;
}
'''
start = source.index('                    KEVENT done;IO_STATUS_BLOCK ios;PIRP request;')
end = source.index('\n                }', start)
source = source[:start] + '                    status=CyclePower(target);\n' + source[end:]
source = source.replace('static NTSTATUS NTAPI Dispatch', power+'\nstatic NTSTATUS NTAPI Dispatch', 1)
# This helper must not accidentally exercise XP's inbox USBHUB path.
start = source.index('                if(target->Type!=IO_TYPE_DEVICE ||')
end = source.index('\n                else{', start)
source = source[:start] + '''                if(target->Type!=IO_TYPE_DEVICE ||
                   !RtlEqualUnicodeString(&target->DriverObject->DriverName,&expected,TRUE))status=STATUS_NOT_SUPPORTED;''' + source[end:]
source = source.replace('RltkResetTest', 'RltkPowerTest').replace('RESETTEST', 'POWERTEST')
source = source.replace('Test-only Windows 2000 helper. Reset a named USBNT PDO through the actual\n * internal USB IOCTL.', 'Test-only USBNT function-power helper. Send D3/D0 to a named PDO.')
probe = (here/'guest_usb_reset_probe.c').read_text().replace('RltkResetTest', 'RltkPowerTest')
probe = probe.replace('rreset.sys', 'upower.sys').replace('RESET_PORT error=', 'FUNCTION_POWER error=')
(out/'upower_driver.c').write_text(source)
(out/'upower_probe.c').write_text(probe)
cc = 'i686-w64-mingw32-gcc' if arch == 'i386' else 'x86_64-w64-mingw32-gcc'
common = ['-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-mno-stack-arg-probe',
          '-mno-sse','-mno-mmx','-fno-asynchronous-unwind-tables','-nostdlib']
common += ['-D_X86_'] if arch == 'i386' else ['-D_AMD64_','-D_WIN64','-mno-red-zone','-msoft-float','-mno-sse2']
kentry = '_DriverEntry@8' if arch == 'i386' else 'DriverEntry'
entry = '_Entry' if arch == 'i386' else 'Entry'
version = '5.0' if arch == 'i386' else '5.2'
# SIMD is forbidden in the kernel helper, but the user-mode probe follows
# the ordinary x64 ABI (the SDK's inline fence requires SSE enabled).
user_common = [flag for flag in common if flag not in
               ('-mno-sse', '-mno-mmx', '-mno-sse2', '-msoft-float', '-mno-red-zone')]
commands = [
    [cc,*common,'-I'+os.environ['MINGW_DDK_INCLUDE'],'-Wl,--subsystem,native',
     '-Wl,--entry,'+kentry,'-Wl,--image-base,0x10000','-Wl,--no-insert-timestamp',
     '-o',str(out/'UPOWER.SYS'),str(out/'upower_driver.c'),'-lntoskrnl','-lhal'],
    [sys.executable,str(here.parent/'USBNT/build/fixpe5.py'),str(out/'UPOWER.SYS'),arch],
    [cc,*user_common,'-Wl,--entry,'+entry,'-Wl,--subsystem,console:'+version,'-Wl,--no-insert-timestamp',
     '-o',str(out/'UPOWER.EXE'),str(out/'upower_probe.c'),'-lkernel32','-ladvapi32','-lsetupapi'],
]
for command in commands:
    subprocess.run(command, check=True)
manifest = {'utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'scope': __doc__, 'commands': commands, 'files': {}}
for path in sorted(out.iterdir()):
    manifest['files'][path.name] = {'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(out/'manifest.json')
