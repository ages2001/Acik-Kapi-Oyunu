"""Execute the production x86 object-release declaration against an ECX ABI stub.

No Windows kernel function is executed. The stub checks the register contract
observed in the supplied NT3.51/2000 exports. An old-stdcall negative control
must fail, so an exported name alone cannot make this test pass.
"""
from pathlib import Path
import os, subprocess, tempfile

src = Path(__file__).resolve().parents[1] / 'RLTKLANT/src/ndiskit.h'
header = src.read_text()
assert 'VOID __attribute__((fastcall)) ObfDereferenceObject(PVOID);' in header
body = r'''
#define RTL_NDIS3
#define __stdcall __attribute__((stdcall))
#define __declspec(x)
#include "ndiskit.h"
__asm__(".globl ObfDereferenceObject\n"
        ".align 4\n"
        ".long 0x1badb002,0,-0x1badb002\n"
        ".globl boot\n"
        "boot:\n"
        "movl $0x200000,%esp\n"
        "call TestMain\n"
        "hlt\n"
        "ObfDereferenceObject:\n"
        "cmpl $0x12345678,%ecx\n"
        "jne bad_contract\n"
        "ret\n"
        "bad_contract:\n"
        "movl $7,%eax\n"
        "movl $0xf4,%edx\n"
        "outl %eax,%dx\n"
        "hlt\n");
void TestMain(void)
{
    unsigned before, after, i, failed;
    __asm__ volatile("movl %%esp,%0" : "=r"(before));
    for (i=0;i<100000;i++) {
        __asm__ volatile("xorl %%ecx,%%ecx" ::: "ecx");
        ObDereferenceObject((void*)0x12345678);
    }
    __asm__ volatile("movl %%esp,%0" : "=r"(after));
    failed=before!=after;
    __asm__ volatile("outl %%eax,%%dx" :: "a"(failed),"d"(0xf4) : "memory");
    __builtin_unreachable();
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-abi-') as td:
    p=Path(td);(p/'test.c').write_text(body)
    for negative in (False, True):
        h=header.replace('VOID __attribute__((fastcall)) ObfDereferenceObject(PVOID);',
                         'VOID STDC ObfDereferenceObject(PVOID);') if negative else header
        (p/'ndiskit.h').write_text(h)
        exe=p/('negative' if negative else 'production')
        subprocess.run(['gcc','-m32','-O2','-ffreestanding','-fno-builtin',
                        '-fno-stack-protector','-fno-pie','-no-pie','-nostdlib',
                        '-Wall','-Wextra','-Werror','-Wl,-Ttext,0x100000,-e,boot',
                        str(p/'test.c'),'-o',str(exe)],check=True)
        qemu=os.environ.get('RLTK_QEMU_I386','/workspace/rltklant-tools/qemu-bin/qemu-system-i386')
        result=subprocess.run([qemu,'-L','/workspace/rltklant-tools/qemu-src/pc-bios',
                               '-machine','pc','-cpu','pentium','-m','16',
                               '-display','none','-nic','none','-serial','none',
                               '-no-reboot','-kernel',str(exe),
                               '-device','isa-debug-exit,iobase=0xf4,iosize=4'],
                              timeout=15,capture_output=True,text=True)
        assert not result.stderr,result.stderr
        assert result.returncode==(15 if negative else 1),result.returncode
print('PASS: 100000 production x86 object releases preserve ECX/ESP; old stdcall negative control rejected')
