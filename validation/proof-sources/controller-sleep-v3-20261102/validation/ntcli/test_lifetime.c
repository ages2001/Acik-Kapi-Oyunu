#define USBNT_CLI_HOST_TEST
#include "../../USBNT/src/ntcli.c"
#include <stdio.h>
static atomic_uint last_id,arrivals,removals;
static void arrived(void *ctx,const UC_DEVINFO *info){(void)ctx;atomic_store(&last_id,info->Id);atomic_fetch_add(&arrivals,1);atomic_store(&in_arrival,1);while(atomic_load(&block_arrival)&&!atomic_load(&release_arrival))OsSleepMs(1);atomic_store(&in_arrival,0);}
static void removed(void *ctx,ULONG id){(void)ctx;assert(id==atomic_load(&last_id));assert(!atomic_load(&in_arrival));atomic_fetch_add(&removals,1);}
static UC_CLIENT client={NULL,arrived,removed};
static void present(void){USB_DEV *d=&UsbDevs[0];memset(d,0,sizeof(*d));d->InUse=d->Ready=1;d->Config=iface;d->ConfigLen=sizeof(iface);}
static void *transfer(void *ctx){UCHAR b[64];ULONG actual=7;(void)ctx;assert(CliTransfer(atomic_load(&last_id),0x81,b,sizeof(b),100,&actual)==UC_ERR_IO);assert(!actual);return NULL;}
static void *detach(void *ctx){(void)ctx;CliDetach(&UsbDevs[0]);atomic_store(&detached,1);return NULL;}
static void *arrival(void *ctx){(void)ctx;CliArrival(&UsbDevs[0]);return NULL;}
static void *unregister_client(void *ctx){(void)ctx;CliUnregister(&client);atomic_store(&unregistered,1);return NULL;}
static void await_flag(atomic_int *flag){int i;for(i=0;i<2000&&!atomic_load(flag);i++)OsSleepMs(1);assert(atomic_load(flag));}
int main(void){pthread_t a,b;ULONG old;UC_DEVINFO info;UCHAR buf[8];ULONG n=123;int i;
 assert(CliRegister(&client)==UC_OK);present();CliArrival(&UsbDevs[0]);old=atomic_load(&last_id);assert(old);
 assert(CliClaim(old,1)==UC_OK);assert(!CliBeginDeviceReset(&UsbDevs[0]));
 assert(CliClaim(old,0)==UC_OK);assert(CliBeginDeviceReset(&UsbDevs[0]));
 assert(!CliBeginDeviceReset(&UsbDevs[0]));assert(CliGetInfo(old,&info)==UC_ERR_BUSY);
 assert(CliControl(old,0,0,0,0,NULL,0,1,&n)==UC_ERR_BUSY);
 CliEndDeviceReset(&UsbDevs[0],-1);assert(CliGetInfo(old,&info)==UC_ERR_IO);
 assert(CliBeginDeviceReset(&UsbDevs[0]));CliAbortDeviceReset(&UsbDevs[0]);
 assert(CliGetInfo(old,&info)==UC_ERR_IO); /* query rollback must preserve failure */
 assert(CliBeginDeviceReset(&UsbDevs[0]));CliEndDeviceReset(&UsbDevs[0],USB_OK);
 assert(CliGetInfo(old,&info)==UC_OK&&info.Id==old);
 assert(CliControl(old,0,0,0,0,NULL,4,1,&n)==UC_ERR_PARAM&&n==0);
 assert(CliGetDescriptor(old,1,0,buf,0x10000,&n)==UC_ERR_PARAM);
 pthread_create(&a,NULL,transfer,NULL);await_flag(&in_transfer);
 assert(!CliBeginDeviceReset(&UsbDevs[0]));
 assert(CliControl(old,0,0,0,0,NULL,0,1,&n)==UC_ERR_BUSY);
 pthread_create(&b,NULL,detach,NULL);OsSleepMs(20);
 assert(!atomic_load(&detached)&&!atomic_load(&closed));assert(CliGetInfo(old,&info)==UC_ERR_NODEV);
 atomic_store(&release_transfer,1);pthread_join(a,NULL);pthread_join(b,NULL);
 assert(atomic_load(&closed)==1&&atomic_load(&removals)==1);
 present();CliArrival(&UsbDevs[0]);old=atomic_load(&last_id);
 assert(GetPipe(&UsbDevs[0],0x81)!=NULL);assert(!CliBeginDeviceReset(&UsbDevs[0]));CliDetach(&UsbDevs[0]);
 for(i=0;i<1000;i++){present();CliArrival(&UsbDevs[0]);assert(atomic_load(&last_id)!=old);assert(CliGetInfo(old,&info)==UC_ERR_NODEV);old=atomic_load(&last_id);CliDetach(&UsbDevs[0]);}
 present();atomic_store(&block_arrival,1);atomic_store(&detached,0);
 pthread_create(&a,NULL,arrival,NULL);await_flag(&in_arrival);
 assert(!CliBeginDeviceReset(&UsbDevs[0]));
 pthread_create(&b,NULL,detach,NULL);OsSleepMs(20);assert(!atomic_load(&detached));
 atomic_store(&release_arrival,1);pthread_join(a,NULL);pthread_join(b,NULL);assert(atomic_load(&detached));
 atomic_store(&release_arrival,0);
 present();pthread_create(&a,NULL,arrival,NULL);await_flag(&in_arrival);
 pthread_create(&b,NULL,unregister_client,NULL);OsSleepMs(20);assert(!atomic_load(&unregistered));
 atomic_store(&release_arrival,1);pthread_join(a,NULL);pthread_join(b,NULL);assert(atomic_load(&unregistered));
 {unsigned count=atomic_load(&removals);CliDetach(&UsbDevs[0]);assert(count==atomic_load(&removals));}
 puts("PASS: actual ntcli.c: stale handles, 1000 reattachments, concurrent transfer/removal, callback rundown, reset/claim/endpoint exclusion, failed reset and retry, invalid lengths");return 0;}
