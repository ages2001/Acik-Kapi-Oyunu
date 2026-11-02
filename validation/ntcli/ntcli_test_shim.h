#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <assert.h>
typedef uint32_t ULONG;
typedef uint16_t USHORT;
typedef uint8_t UCHAR;
typedef int BOOLEAN;
typedef int OS_IRQL;
#define USB_MAX_DEVICES 64
#define USB_OK 0
#define USB_DT_INTERFACE 4
#define USB_DT_ENDPOINT 5
typedef struct { UCHAR Index,InUse,Gone,Ready,Speed; UCHAR DevDesc[18]; UCHAR *Config; ULONG ConfigLen; char Mfg[32],Product[48]; } USB_DEV;
typedef struct { USB_DEV *Dev; } USB_PIPE;
#include "../../USBNT/src/usbntcli.h"
static USB_DEV UsbDevs[USB_MAX_DEVICES];
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static atomic_int in_transfer,release_transfer,closed,block_arrival,in_arrival,release_arrival,unregistered,detached;
static OS_IRQL OsLock(void){assert(!pthread_mutex_lock(&lock));return 0;}
static void OsUnlock(OS_IRQL unused){(void)unused;assert(!pthread_mutex_unlock(&lock));}
static void OsSleepMs(ULONG ms){struct timespec t={ms/1000,(ms%1000)*1000000};nanosleep(&t,NULL);}
#define UsbMemSet memset
#define UsbMemCpy memcpy
#define OsAlloc malloc
#define OsFree free
static void *OsDmaAlloc(ULONG n,ULONG *phys){*phys=0;return malloc(n);}
static void OsDmaFree(void *p,ULONG n){(void)n;free(p);}
static UCHAR iface[]={9,4,0,0,1,255,0,0,0};
static UCHAR ep[]={7,5,0x81,2,64,0,0};
static const UCHAR *UsbFindDesc(const UCHAR *p,const UCHAR *end,UCHAR type){(void)p;(void)end;return type==4?iface:ep;}
static int UsbOpenPipe(USB_DEV *d,USB_PIPE *p,const UCHAR *e){(void)e;p->Dev=d;return 0;}
static void UsbClosePipe(USB_PIPE *p){(void)p;assert(!atomic_load(&in_transfer));atomic_fetch_add(&closed,1);}
static int UsbControl(USB_DEV *d,UCHAR t,UCHAR r,USHORT v,USHORT i,USHORT n,void *b,ULONG ms,ULONG *a){(void)d;(void)t;(void)r;(void)v;(void)i;(void)ms;if(n)memset(b,0,n);if(a)*a=n;return 0;}
static int UsbGetDescriptor(USB_DEV *d,UCHAR t,UCHAR i,USHORT l,void *b,USHORT n,ULONG *a){(void)l;return UsbControl(d,t,0,0,i,n,b,1,a);}
static int UsbTransfer(USB_PIPE *p,void *b,ULONG phys,ULONG n,ULONG ms,ULONG *a){(void)p;(void)b;(void)phys;(void)n;(void)ms;atomic_store(&in_transfer,1);while(!atomic_load(&release_transfer))OsSleepMs(1);*a=0;atomic_store(&in_transfer,0);return -1;}
