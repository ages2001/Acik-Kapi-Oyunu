"""Compile production QEMU USB callbacks against a packet-granularity harness."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'RLTKLANT/qemu/hw-usb'
source=(root/'dev-rtl8153.c').read_text()
def function(name):
 m=re.search(r'static [\w ]+ '+name+r'\([^;]*?\)\s*\{',source);assert m,name
 i=m.end();depth=1
 while depth:depth+=(source[i]=='{')-(source[i]=='}');i+=1
 return source[m.start():i]
struct=source[source.index('struct USBRtl8153State {'):source.index('\n};',source.index('struct USBRtl8153State {'))+3]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "rtl815x-core.h"
#include "rtl8150-core.h"
#include "rtl815x-transfer.h"
#define MIN(a,b) ((a)<(b)?(a):(b))
#define USB_TOKEN_OUT 0
#define USB_TOKEN_IN 1
#define USB_RET_NAK -1
#define USB_RET_STALL -2
#define EndpointRequest 0x8200
#define EndpointOutRequest 0x0200
#define USB_REQ_GET_STATUS 0
#define USB_REQ_CLEAR_FEATURE 1
#define USB_REQ_SET_FEATURE 3
typedef struct {int nr,max_packet_size;} USBEndpoint;
typedef struct {int configuration;} USBDevice;
typedef struct QEMUTimer QEMUTimer;
static void timer_del(QEMUTimer*t){(void)t;}
typedef struct {USBEndpoint*ep;int pid,status;struct {unsigned size;} iov;unsigned actual_length;uint8_t *data;} USBPacket;
typedef struct USBRtl8153State USBRtl8153State;
typedef struct {USBRtl8153State *owner;int link_down;} NetClientState;
typedef NetClientState NICState;
typedef struct {uint8_t mac[6];} NICConf;
#define USB_RTL8153(p) ((USBRtl8153State*)(p))
static uint32_t ldl_le_p(const uint8_t*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void stl_le_p(uint8_t*p,uint32_t v){p[0]=v;p[1]=v>>8;p[2]=v>>16;p[3]=v>>24;}
static void usb_packet_copy(USBPacket*p,void*buf,unsigned n){assert(n<=p->iov.size);if(p->pid==USB_TOKEN_IN)memcpy(p->data,buf,n);else memcpy(buf,p->data,n);p->actual_length=n;}
static void *qemu_get_nic_opaque(NetClientState*n){return n->owner;}
static NetClientState *qemu_get_queue(NICState*n){return n;}
static USBEndpoint *usb_ep_get(USBDevice*d,int pid,int ep){(void)d;(void)pid;(void)ep;return NULL;}
static void usb_wakeup(USBEndpoint*e,int stream){(void)e;(void)stream;}
static uint8_t wire[1600],queued[1600];static unsigned wirelen,queuedlen,sends,flushes;
static void qemu_send_packet(NetClientState*n,const uint8_t*p,unsigned len){(void)n;assert(len<=sizeof(wire));memcpy(wire,p,len);wirelen=len;sends++;}
static void qemu_flush_queued_packets(NetClientState*n);
static void qemu_purge_queued_packets(NetClientState*n){(void)n;queuedlen=0;}
'''
callbacks='\n'.join(function(n) for n in ['rtl_can_receive','rtl_receive','rtl_link_status_changed','rtl_handle_reset','rtl_check_stopped','rtl_endpoint_control','rtl_handle_data'])
tests=r'''
static void qemu_flush_queued_packets(NetClientState*n){flushes++;if(queuedlen&&rtl_can_receive(n)){unsigned len=queuedlen;queuedlen=0;assert(rtl_receive(n,queued,len)==len);}}
static void initialize(USBRtl8153State*s,NICState*n,int mode){memset(s,0,sizeof(*s));n->owner=s;n->link_down=0;s->nic=n;s->legacy=mode==0;s->v2=mode==2;s->core.link_up=s->core.tx_enabled=s->core.rx_enabled=1;s->core8150.link_up=s->core8150.tx_enabled=s->core8150.rx_enabled=1;sends=wirelen=flushes=queuedlen=0;}
int main(void){
 USBRtl8153State s;NICState nic;USBEndpoint ep;USBPacket p;uint8_t frame[1514],encoded[2048],got[2048],expected[2048];unsigned len,mode,packet,at,n,total,full;int rx;
 for(at=0;at<sizeof(frame);at++)frame[at]=(uint8_t)(at*37+3);
 for(mode=0;mode<3;mode++)for(packet=64;packet<=512;packet*=8)for(len=60;len<=1514;len++){
  initialize(&s,&nic,mode);ep.nr=2;ep.max_packet_size=packet;memset(&p,0,sizeof(p));p.ep=&ep;p.pid=USB_TOKEN_OUT;
  memset(encoded,0,sizeof(encoded));n=mode==0?0:mode==1?8:16;
  if(mode){stl_le_p(encoded,0xc0000000u|len);if(mode==2){stl_le_p(encoded+8,len<<4);stl_le_p(encoded+12,0xa8000000u);}}
  memcpy(encoded+n,frame,len);total=n+len;if(mode==0&&!(total%packet))total++;
  for(at=0;at<total;at+=n){n=MIN(packet,total-at);p.data=encoded+at;p.iov.size=n;p.actual_length=0;rtl_handle_data(&s.dev,&p);assert(!p.status&&p.actual_length==n);if(at+n<total)assert(!sends);}
  assert(sends==1&&wirelen==len+(mode==0&&!(len%packet))&&!memcmp(wire,frame,len));
 }
 /* Real callback queue rearming and exact-maxpacket RX termination. */
 for(mode=0;mode<3;mode++)for(packet=64;packet<=512;packet*=8)for(len=14;len<=1514;len++){
  initialize(&s,&nic,mode);ep.nr=1;ep.max_packet_size=packet;memset(&p,0,sizeof(p));p.ep=&ep;p.pid=USB_TOKEN_IN;p.iov.size=packet;p.data=got;
  assert(rtl_receive(&nic,frame,len)==len);full=s.rxlen;memcpy(expected,s.rxbuf,full);
  memcpy(queued,frame,len);queuedlen=len;assert(!rtl_can_receive(&nic));total=0;
  do{p.status=0;p.actual_length=0;rtl_handle_data(&s.dev,&p);assert(!p.status);n=p.actual_length;assert(!memcmp(got,expected+total,n));total+=n;if(n==packet)assert(!flushes);}while(n==packet);
  assert(total==full&&flushes==1&&queuedlen==0&&s.rxlen==full); /* second queued frame is now available */
  nic.link_down=1;rtl_link_status_changed(&nic);assert(!s.rxlen&&!s.rxzlp&&!s.tx.count&&!rtl_can_receive(&nic));
 }
 initialize(&s,&nic,0);s.tx.count=64;s.rxlen=128;s.rxzlp=1;queuedlen=64;rtl_handle_reset(&s.dev);assert(!s.tx.count&&!s.rxlen&&!s.rxzlp&&!queuedlen);
 initialize(&s,&nic,0);s.tx.count=64;s.rxlen=128;s.rxzlp=1;queuedlen=64;s.core8150.tx_enabled=0;rtl_check_stopped(&s);assert(!s.tx.count&&!s.rxlen&&!s.rxzlp&&!queuedlen);
 /* Standard endpoint halt persists until a successful clear, independent
    of the bus core's transient queue-halt flag. */
 for(mode=0;mode<3;mode++){
  unsigned endpoint,bit;uint8_t data[2];
  initialize(&s,&nic,mode);s.dev.configuration=1;
  for(bit=1;bit<=4;bit*=2){
   endpoint=bit==1?0x81:bit==2?2:0x83;
   memset(&p,0,sizeof(p));p.ep=&ep;
   assert(rtl_endpoint_control(&s,&p,0x0203,0,endpoint,0,data)&&!p.status);
   assert(s.halted_endpoints&bit);
   assert(rtl_endpoint_control(&s,&p,0x8200,0,endpoint,2,data)&&data[0]==1&&data[1]==0&&p.actual_length==2);
   ep.nr=endpoint&15;p.pid=endpoint&0x80?USB_TOKEN_IN:USB_TOKEN_OUT;
   rtl_handle_data(&s.dev,&p);assert(p.status==USB_RET_STALL);
   p.status=0;s.rxlen=128;s.rxptr=64;
   assert(rtl_endpoint_control(&s,&p,0x0201,0,endpoint,0,data)&&!p.status&&!(s.halted_endpoints&bit));
   assert(s.rxlen==128&&s.rxptr==64); /* USB clear does not discard queued payload */
   assert(rtl_endpoint_control(&s,&p,0x8200,0,endpoint,2,data)&&!data[0]);
  }
  for(endpoint=0;endpoint<256;endpoint++)if(endpoint!=0x81&&endpoint!=2&&endpoint!=0x83){
   p.status=0;assert(rtl_endpoint_control(&s,&p,0x0203,0,endpoint,0,data)&&p.status==USB_RET_STALL);
  }
  p.status=0;assert(rtl_endpoint_control(&s,&p,0x8200,0,0,2,data)&&!p.status&&!data[0]);
  p.status=0;assert(rtl_endpoint_control(&s,&p,0x0201,1,0x81,0,data)&&p.status==USB_RET_STALL);
  s.dev.configuration=0;p.status=0;assert(rtl_endpoint_control(&s,&p,0x0203,0,0x81,0,data)&&p.status==USB_RET_STALL);
  s.halted_endpoints=7;rtl_handle_reset(&s.dev);assert(!s.halted_endpoints);
 }
 puts("PASS: production QEMU endpoint halt/control/data/reset semantics");
 puts("PASS: production QEMU callbacks: 8730 fragmented TX transfers, 9006 RX boundary/queue schedules, reset/link purge");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='qemu-usb-packets-') as d:
 p=Path(d)/'test.c';p.write_text(prefix+struct+'\n'+callbacks+'\n'+tests)
 exe=Path(d)/'test';subprocess.run(['gcc','-std=c99','-g','-Wall','-Wextra','-Wno-unused-variable','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
