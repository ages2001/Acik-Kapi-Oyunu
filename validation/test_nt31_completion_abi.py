"""Execute the production full-MAC completion macros on a Pentium QEMU CPU.

The callbacks validate argument order, protocol context, stack cleanup, and
exact status/reset counts. This is an ABI harness, not a Windows reset test.
"""
from pathlib import Path
import subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
body=r'''
#define __stdcall __attribute__((stdcall))
#define __declspec(x)
#include "ndisnt31.h"
#include "nt31ndisabi.h"
__asm__(".align 4\n.long 0x1badb002,0,-0x1badb002\n.globl boot\nboot:\nmovl $0x200000,%esp\ncall TestMain\nhlt\n");
static volatile unsigned resets,statuses,completed;
static void fail(void){__asm__ volatile("outl %%eax,%%dx"::"a"(7),"d"(0xf4));__builtin_unreachable();}
static VOID STDC __attribute__((noinline)) reset(NDIS_HANDLE c,NDIS_STATUS s){if(c!=(void*)0x12345678||s!=(NDIS_STATUS)0xc001000dU)fail();resets++;}
static VOID STDC __attribute__((noinline)) status(NDIS_HANDLE c,NDIS_STATUS s,PVOID data,UINT n){if(c!=(void*)0x12345678||s!=(NDIS_STATUS)0x40010004U||data||n)fail();statuses++;}
static VOID STDC __attribute__((noinline)) complete(NDIS_HANDLE c){if(c!=(void*)0x12345678)fail();completed++;}
static void __attribute__((noinline)) invoke(void*p){N31CompleteReset(p,(NDIS_STATUS)0xc001000dU);N31IndicateResetStatus(p,(NDIS_STATUS)0x40010004U);}
void TestMain(void){N31_PROTOCOL_PREFIX protocol;N31_OPEN_PREFIX open;unsigned i,before,after;
 unsigned char*p=(unsigned char*)&protocol;for(i=0;i<sizeof(protocol);i++)p[i]=0;
 p=(unsigned char*)&open;for(i=0;i<sizeof(open);i++)p[i]=0;
 protocol.Characteristics.ResetComplete=reset;protocol.Characteristics.Status=status;protocol.Characteristics.StatusComplete=complete;
 open.Protocol=&protocol;open.ProtocolContext=(void*)0x12345678;
 __asm__ volatile("movl %%esp,%0":"=r"(before));
 for(i=0;i<100000;i++)invoke(&open);
 __asm__ volatile("movl %%esp,%0":"=r"(after));
 if(before!=after||resets!=100000||statuses!=100000||completed!=100000)fail();
 __asm__ volatile("outl %%eax,%%dx"::"a"(0),"d"(0xf4));__builtin_unreachable();
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-nt31-completion-') as td:
    p=Path(td);(p/'test.c').write_text(body);exe=p/'test'
    subprocess.run(['gcc','-m32','-march=pentium','-O2','-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-pie','-no-pie','-nostdlib','-Wall','-Wextra','-Werror','-Wl,-Ttext,0x100000,-e,boot','-I',str(src),str(p/'test.c'),'-o',str(exe)],check=True)
    result=subprocess.run(['/workspace/rltklant-tools/qemu-bin/qemu-system-i386','-L','/workspace/rltklant-tools/qemu-src/pc-bios','-machine','pc','-cpu','pentium','-m','16','-display','none','-nic','none','-serial','none','-no-reboot','-kernel',str(exe),'-device','isa-debug-exit,iobase=0xf4,iosize=4'],capture_output=True,text=True,timeout=15)
    assert not result.stderr and result.returncode==1,(result.returncode,result.stderr)
print('PASS: 100000 NT3.1 full-MAC reset/status/status-complete macro calls preserve context, arguments and ESP on Pentium')
