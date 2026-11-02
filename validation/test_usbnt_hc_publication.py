"""The actual AddControllerAt publication block must drain table readers."""
from pathlib import Path
import subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
p=Path(sys.argv[1]) if len(sys.argv)>1 else root/'USBNT/src/usbcore.c'
s=p.read_text();base=s.index('static USB_HC *AddControllerAt(');a=s.index('    UsbHcs[hc->Index] = hc;',base);b=s.index('    UsbLog(LOG_INFO, "found ',a)
lock=s.rfind('    irql = OsLock();',base,a)
if lock!=-1:a=lock
block=s[a:b]
prefix=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <time.h>
typedef struct {int Index;} USB_HC;
typedef int OS_IRQL;
static USB_HC *UsbHcs[1];static int UsbHcCount;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static atomic_int entered,finished;
static OS_IRQL OsLock(void){assert(!pthread_mutex_lock(&mutex));return 0;}
static void OsUnlock(OS_IRQL k){assert(!k&&!pthread_mutex_unlock(&mutex));}
static void pause_ms(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void publish(USB_HC*hc,USB_HC*Replace){
'''
main=r'''
}
static void*publisher(void*p){atomic_store(&entered,1);publish(p,UsbHcs[0]);atomic_store(&finished,1);return NULL;}
int main(void){USB_HC old={0},next={0};pthread_t thread;int i;OS_IRQL k;
 for(i=0;i<100;i++){UsbHcs[0]=&old;UsbHcCount=1;atomic_store(&entered,0);atomic_store(&finished,0);k=OsLock();assert(!pthread_create(&thread,NULL,publisher,&next));while(!atomic_load(&entered))pause_ms();pause_ms();assert(!atomic_load(&finished)&&UsbHcs[0]==&old);OsUnlock(k);pthread_join(thread,NULL);assert(UsbHcs[0]==&next&&UsbHcCount==1);}
 UsbHcCount=0;publish(&next,NULL);assert(UsbHcCount==1&&UsbHcs[0]==&next);
 puts("PASS: 100 actual controller table publication schedules; replacement waits for readers, retains slot count and publishes before retirement");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c';exe=Path(tmp)/'test';c.write_text(prefix+('OS_IRQL irql;\n' if lock!=-1 else '')+block+main)
 subprocess.run(['gcc','-std=c11','-pthread','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
