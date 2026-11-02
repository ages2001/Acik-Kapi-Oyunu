"""Actual root-port polling must resume after a sleeping EHCI owner is removed."""
from pathlib import Path
import re, subprocess, sys, tempfile
root=Path(__file__).resolve().parents[1]
p=Path(sys.argv[1]) if len(sys.argv)>1 else root/'USBNT/src/usbcore.c'
s=p.read_text();m=re.search(r'void UsbPortPoll\([^;{]*\)\s*\{',s);assert m
end=m.end();depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
prefix=r'''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
typedef unsigned char UCHAR;typedef unsigned long ULONG;
#define PS_CONNECT 1
#define PS_ENABLE 2
#define PS_CHANGE 4
#define PS_OTHER_OWNER 8
#define PS_SS_PORT 16
#define USB_SPEED_FULL 1
#define USB_OK 0
#define USB_PORT_HANDOFF 1
#define UsbLog(...) ((void)0)
typedef struct {int unused;} USB_DEV;
typedef struct HC USB_HC;
typedef struct {ULONG(*PortStatus)(USB_HC*,int);void(*PortClearChange)(USB_HC*,int);int(*PortReset)(USB_HC*,int,UCHAR*);void(*PortDisable)(USB_HC*,int);} OPS;
struct HC {OPS*Ops;USB_HC*Ehci;int Sleeping,Removed,NumPorts;USB_DEV*RootDev[4];int PortRetry[4];};
static int reads;
static ULONG status(USB_HC*h,int p){assert(p>0&&p<=h->NumPorts);reads++;return 0;}
static void clear(USB_HC*h,int p){(void)h;(void)p;assert(0);}
static int reset(USB_HC*h,int p,UCHAR*s){(void)h;(void)p;(void)s;assert(0);return -1;}
static void UsbDevDetach(USB_DEV*d){(void)d;assert(0);}
static void OsSleepMs(int n){(void)n;assert(0);}
static USB_DEV*UsbDevAttach(USB_HC*h,USB_DEV*d,int p,UCHAR s){(void)h;(void)d;(void)p;(void)s;assert(0);return NULL;}
'''
main=r'''
int main(void){OPS ops={status,clear,reset,clear};USB_HC ehci={0},companion={0};companion.Ops=&ops;companion.NumPorts=3;companion.Ehci=&ehci;
 ehci.Sleeping=1;UsbPortPoll(&companion);assert(!reads);
 ehci.Removed=1;UsbPortPoll(&companion);assert(reads==3); /* old source leaves the companion blocked */
 reads=0;companion.Sleeping=1;UsbPortPoll(&companion);assert(!reads);
 companion.Sleeping=0;companion.Ehci=NULL;UsbPortPoll(&companion);assert(reads==3);
 reads=0;companion.Ehci=&ehci;ehci.Sleeping=ehci.Removed=0;UsbPortPoll(&companion);assert(reads==3);
 puts("PASS: actual port polling preserves live EHCI sleep ownership and resumes after EHCI removal; own sleep still blocks MMIO");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
    c=Path(tmp)/'test.c';exe=Path(tmp)/'test';c.write_text(prefix+s[m.start():end]+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
