"""Delay a real UsbPollAll call across the actual controller park/release path."""
from pathlib import Path
import re,subprocess,sys,tempfile
root=Path(__file__).resolve().parents[1]
header=Path(sys.argv[1]) if len(sys.argv)>1 else root/'USBNT/src/usbsleep.h'
def fn(text,name):
 m=re.search(r'(?:static )?(?:int|void) '+name+r'\([^;{]*\)\s*\{',text);assert m
 e=m.end();depth=1
 while depth:depth+=(text[e]=='{')-(text[e]=='}');e+=1
 return text[m.start():e]
park=fn(header.read_text(),'SleepParkController');core=(root/'USBNT/src/usbcore.c').read_text()
prefix=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
typedef int BOOLEAN,OS_IRQL;
#define USB_ERR_IO -1
#define USB_ERR_BUSY -2
#define USB_ERR_PARAM -3
#define FALSE 0
typedef struct HC USB_HC;typedef struct DEV USB_DEV;
typedef struct X USB_XFER;struct X{USB_XFER*DoneNext;void(*Complete)(USB_XFER*);};
struct DEV {int Ep0;};
typedef struct {void(*Stop)(USB_HC*);int(*Release)(USB_HC*);void(*DevFree)(USB_DEV*);void(*Poll)(USB_HC*);} OPS;
struct HC {OPS*Ops;atomic_int Running;int NumPorts;USB_DEV*RootDev[2];int*Priv;};
static USB_HC*UsbHcs[1];static int UsbHcCount=1;static USB_XFER *DoneHead,*DoneTail;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static atomic_int polling,allow_poll,stopped,release_calls,park_done;
static int direct_release;
static OS_IRQL OsLock(void){assert(!pthread_mutex_lock(&mutex));return 0;}
static void OsUnlock(OS_IRQL k){assert(!k&&!pthread_mutex_unlock(&mutex));}
static void pause_ms(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void SleepCloseWindows(USB_DEV*d){(void)d;assert(0);}
static void SleepEndNative(USB_DEV*d,int r){(void)d;(void)r;assert(0);}
static void UsbClosePipe(int*p){(void)p;assert(0);}
static void pollhc(USB_HC*h){int*p=h->Priv;assert(p&&*p==123);atomic_store(&polling,1);while(!atomic_load(&allow_poll))pause_ms();assert(*p==123);atomic_store(&polling,0);}
static void stophc(USB_HC*h){atomic_store(&h->Running,0);atomic_store(&stopped,1);}
static int releasehc(USB_HC*h){assert(!atomic_load(&polling));assert(h->Priv&&*h->Priv==123);free(h->Priv);h->Priv=NULL;atomic_fetch_add(&release_calls,1);return 0;}
'''
main=r'''
static void*pollthread(void*p){(void)p;UsbPollAll();return NULL;}
static void*parkthread(void*p){if(direct_release){stophc(p);assert(!UsbReleaseController(p));}else assert(!SleepParkController(p,FALSE));atomic_store(&park_done,1);return NULL;}
int main(void){int i;OPS ops={stophc,releasehc,NULL,pollhc};USB_HC h={0};pthread_t a,b;h.Ops=&ops;UsbHcs[0]=&h;
 for(i=0;i<200;i++){direct_release=i>=100;atomic_store(&polling,0);atomic_store(&allow_poll,0);atomic_store(&stopped,0);atomic_store(&release_calls,0);atomic_store(&park_done,0);h.Priv=malloc(sizeof(int));assert(h.Priv);*h.Priv=123;atomic_store(&h.Running,1);
 assert(!pthread_create(&a,NULL,pollthread,NULL));while(!atomic_load(&polling))pause_ms();assert(!pthread_create(&b,NULL,parkthread,&h));while(!atomic_load(&stopped))pause_ms();pause_ms();assert(!atomic_load(&release_calls)&&!atomic_load(&park_done));atomic_store(&allow_poll,1);pthread_join(a,NULL);pthread_join(b,NULL);assert(atomic_load(&release_calls)==1&&atomic_load(&park_done)&&!h.Priv);UsbPollAll();}
 puts("PASS: 200 delayed production UsbPollAll/park and shared-release schedules; HCD memory survives until the active poll returns, then is released once");return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'test.c';exe=Path(tmp)/'test';c.write_text(prefix+fn(core,'RunCompletions')+fn(core,'UsbPollAll')+fn(core,'UsbReleaseController')+park+main)
 subprocess.run(['gcc','-std=c11','-pthread','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
