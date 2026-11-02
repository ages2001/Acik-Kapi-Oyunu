"""Production PnP metadata snapshots remain valid after controller retirement."""
from pathlib import Path
import re,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1]
p=Path(sys.argv[1]) if len(sys.argv)>1 else r/'USBNT/src/wdmtree.c';s=p.read_text()
def fn(name):
 m=re.search(r'static USB_HC \*'+name+r'\([^;{]*\)\s*\{',s);assert m
 end=m.end();depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[m.start():end]
old='USB_HC *Snapshot' not in fn('HcOf')
prefix=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
typedef int OS_IRQL;
typedef struct {int Removed,Bus,Dev,Fn,Type,VendorId,DeviceId;} USB_HC;
typedef struct {int InUse,Gone,Gen;USB_HC*Hc;} USB_DEV;
typedef struct {void*DeviceExtension;} DEVICE,*PDEVICE_OBJECT;
typedef struct {int Bus,Dev,Fn,TGen;USB_DEV*TDev;PDEVICE_OBJECT TOwner;} WDM_EXT;
static USB_HC*UsbHcs[1];static int UsbHcCount=1;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static atomic_int copying,allow_copy,retired,started;
static int mode;static WDM_EXT ext,node;static DEVICE fdo;static USB_DEV device;
static USB_HC snapshot,*returned,*oldhc;
static OS_IRQL OsLock(void){assert(!pthread_mutex_lock(&mutex));return 0;}
static void OsUnlock(OS_IRQL k){assert(!k&&!pthread_mutex_unlock(&mutex));}
static void pause_ms(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void UsbMemCpy(void*d,const void*s,size_t n){atomic_store(&copying,1);while(!atomic_load(&allow_copy))pause_ms();memcpy(d,s,n);}
'''
main=r'''
static void*reader(void*p){(void)p;RETURNED;return NULL;}
static void*retire(void*p){OS_IRQL k;(void)p;atomic_store(&started,1);k=OsLock();device.Gone=1;UsbHcs[0]=NULL;OsUnlock(k);free(oldhc);atomic_store(&retired,1);return NULL;}
int main(void){int i;pthread_t a,b;fdo.DeviceExtension=&ext;ext.Bus=2;ext.Dev=3;ext.Fn=1;node.TOwner=&fdo;device.InUse=1;device.Gen=node.TGen=7;
 for(i=0;i<200;i++){mode=i%2;node.TDev=mode?&device:NULL;atomic_store(&copying,0);atomic_store(&allow_copy,0);atomic_store(&retired,0);atomic_store(&started,0);oldhc=malloc(sizeof(*oldhc));assert(oldhc);*oldhc=(USB_HC){0,2,3,1,2,0x8086,0x24cd};UsbHcs[0]=device.Hc=oldhc;device.Gone=0;
 OLDCASE
 assert(!pthread_create(&a,NULL,reader,NULL));while(!atomic_load(&copying))pause_ms();assert(!pthread_create(&b,NULL,retire,NULL));while(!atomic_load(&started))pause_ms();pause_ms();assert(!atomic_load(&retired));atomic_store(&allow_copy,1);pthread_join(a,NULL);pthread_join(b,NULL);assert(returned==&snapshot&&snapshot.VendorId==0x8086&&snapshot.DeviceId==0x24cd&&snapshot.Bus==2);assert(atomic_load(&retired));}
 node.TDev=&device;device.Gone=1;STALE_GONE
 device.Gone=0;device.Gen=node.TGen+1;STALE_GONE
 device.Gen=node.TGen;device.InUse=0;STALE_GONE
 puts("PASS: 200 actual PnP controller snapshot/retirement schedules; metadata survives free and copying drains before retirement");return 0;}
'''
if old:
 main=main.replace('RETURNED','returned=mode?NodeHc(&node):HcOf(&fdo)')
 main=main.replace('OLDCASE','returned=HcOf(&fdo);assert(returned==&snapshot);').replace('STALE_GONE','')
else:
 main=main.replace('RETURNED','returned=mode?NodeHc(&node,&snapshot):HcOf(&fdo,&snapshot)').replace('OLDCASE','').replace('STALE_GONE','assert(NodeHc(&node,&snapshot)==NULL);')
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c';exe=Path(tmp)/'test';c.write_text(prefix+fn('HcOf')+fn('NodeHc')+main)
 subprocess.run(['gcc','-std=c11','-pthread','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
