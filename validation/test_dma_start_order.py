"""Exercise both production C+ initializers against eager DMA and removal."""
from pathlib import Path
import re,subprocess,tempfile
src=Path(__file__).resolve().parents[1]/'RLTKLANT/src'
def function(path,name):
    text=(src/path).read_text();m=re.search(r'static BOOLEAN '+name+r'\([^;{]*\)\s*\{',text)
    assert m;at=m.end();depth=1
    while depth:
        depth+=(text[at]=='{')-(text[at]=='}');at+=1
    return text[m.start():at]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef uint32_t ULONG;typedef uint8_t UCHAR;typedef uint16_t USHORT;
typedef unsigned UINT;typedef int BOOLEAN;typedef void*PVOID;
#define TRUE 1
#define FALSE 0
#define NdisMoveMemory memcpy
#define NdisZeroMemory(p,n) memset(p,0,n)
#include "rltkhw.h"
#include "macseq.h"
typedef struct {struct {ULONG LowPart,HighPart;}u;} PHYS;
typedef struct {ULONG IoBase,DeviceId,LinkSpeed,Is8125,PendingIsr;
 UCHAR CurrentAddr[6],PermanentAddr[6],Mac[6];
 PRCP_DESC TxDesc,RxDesc;PHYS TxDescPhys,RxDescPhys;
 PVOID MiniportHandle;UCHAR*CpTxBuf[RCP_NUM_TX_DESC],*CpRxBuf[RCP_NUM_RX_DESC];
 PHYS CpTxBufPhys[RCP_NUM_TX_DESC],CpRxBufPhys[RCP_NUM_RX_DESC];
 UCHAR*RxRing,*TxBuf[R8139_TX_DESCS];PHYS RxRingPhys,TxBufPhys[R8139_TX_DESCS];UINT RxOffset,TxCur,TxDirty;
 ULONG CpTxP[RCP_NUM_TX_DESC],CpRxP[RCP_NUM_RX_DESC];
 UINT CpTxHead,CpTxTail,CpRxCur,RxRecoveryNeeded;
} Adapter,*PRLTK_ADAPTER;
/* NT3.1 uses 32-bit physical addresses directly. */
typedef struct {ULONG IoBase,DeviceId,LinkSpeed;UCHAR Mac[6];int PciHardwareInitialized;
 PRCP_DESC TxDesc,RxDesc;ULONG TxDescPhys,RxDescPhys;
 ULONG CpTxP[RCP_NUM_TX_DESC],CpRxP[RCP_NUM_RX_DESC];UINT CpTxHead,CpRxCur;
 ULONG RxRingPhys,TxPhys[4];UINT RxOffset,TxCur;
} OldAdapter,*PN31_ADAPTER;
#define RX_ALLOC (R8139_RXBUF_LEN+16+1536)
#define TX_ALLOC 1536
#define N31_TXN 4
#define CP_TX_RING_BYTES (RCP_NUM_TX_DESC*sizeof(RCP_DESC))
#define CP_RX_RING_BYTES (RCP_NUM_RX_DESC*sizeof(RCP_DESC))
static unsigned regs[256],written[256],enabled,commit,absent,allocs,fail,legacy39,prepared,prepare_fail;
static RCP_DESC*rx;
static void write_reg(unsigned reg,unsigned value){
 if(reg==RCP_CR&&(value&(RCP_CR_TE|RCP_CR_RE))){unsigned i;
  if(legacy39){
   assert(written[R8139_RBSTART]&&written[R8139_CAPR]&&commit&&!absent);
   for(i=0;i<4;i++)assert(written[R8139_TSAD0+4*i]&&regs[R8139_TSAD0+4*i]);
  }else{
  assert(written[RCP_TNPDS]&&written[RCP_TNPDS+4]&&written[RCP_RDSAR]&&written[RCP_RDSAR+4]);
  assert(written[RCP_RMS]&&commit&&!absent);
  assert(regs[RCP_TNPDS]&&regs[RCP_RDSAR]&&regs[RCP_RMS]==RCP_RX_BUF_SIZE);
  for(i=0;i<RCP_NUM_RX_DESC;i++)assert((rx[i].opts1&RCP_DESC_OWN)&&rx[i].addr_lo);
  }
  enabled++;
 }
 written[reg]=1;regs[reg]=value;
}
static unsigned read_reg(unsigned reg){if(reg==(legacy39?R8139_RBSTART:RCP_RDSAR)){commit++;if(absent)return UINT32_MAX;}return regs[reg];}
#define out8 write_reg
#define out16 write_reg
#define out32 write_reg
#define in8 read_reg
#define in32 read_reg
#define RtlWrite8(a,r,v) ((void)(a),write_reg(r,v))
#define RtlWrite16 RtlWrite8
#define RtlWrite32 RtlWrite8
#define RtlRead8(a,r) ((void)(a),read_reg(r))
#define RtlRead32 RtlRead8
static void NdisMAllocateSharedMemory(void*h,unsigned size,int cached,void**p,PHYS*physical){
 (void)h;(void)cached;allocs++;if(allocs==fail){*p=NULL;return;}
 *p=calloc(1,size);assert(*p);physical->u.LowPart=0x10000+allocs*0x10000;
 if(allocs==2)rx=*p;
}
static int R39Reset(Adapter*a){(void)a;return 1;}
static void R39FreeDma(Adapter*a){assert(!a->RxRing);}
static void R39WriteMac(Adapter*a){(void)a;}
static void R39SetRxMode(Adapter*a){(void)a;}
static void Rtl8139ConfigureMedia(Adapter*a){(void)a;}
static int r8139_reset(OldAdapter*a){(void)a;return 1;}
static void r8139_setfilter(OldAdapter*a){(void)a;}
static int CpReset(Adapter*a){(void)a;return 1;}
static void CpFreeDma(Adapter*a){assert(!a->TxDesc&&!a->RxDesc);}
static int n31_mac_hardware(OldAdapter*a,enum RTL_MAC_HARDWARE_STAGE stage){assert(!enabled);if(stage==0)a->PciHardwareInitialized=1;return 1;}
static int RtlInitializePciHardware(Adapter*a){(void)a;assert(!enabled&&!allocs);return 1;}
static int RtlPreparePci(Adapter*a){(void)a;assert(!enabled&&!allocs);prepared++;return !prepare_fail;}
static void CpWriteMac(Adapter*a){(void)a;}
static void CpSetRxMode(Adapter*a){(void)a;}
static void CpIsrAck(Adapter*a,unsigned v){(void)a;(void)v;}
static void CpImr(Adapter*a,unsigned v){(void)a;(void)v;}
static int RtlConfigurePhy(Adapter*a){(void)a;return 1;}
static int cplus_reset(OldAdapter*a){(void)a;return 1;}
static int n31_initialize_phy(OldAdapter*a){(void)a;assert(!enabled);return 1;}
static int cp_is25(OldAdapter*a){return a->DeviceId==0x8127;}
static void cplus_setfilter(OldAdapter*a){(void)a;}
static void dbg(const char*s){(void)s;}
static void dbgh(unsigned n){(void)n;}
static void clear(void){memset(regs,0,sizeof(regs));memset(written,0,sizeof(written));enabled=commit=allocs=prepared=0;}
'''
main=r'''
int main(void){Adapter a;OldAdapter b;unsigned i,j,k;
 for(j=0;j<2;j++)for(k=0;k<2;k++){
  clear();absent=j;fail=0;memset(&a,0,sizeof(a));a.Is8125=k;
  assert(CpInit(&a)==!j);assert(enabled==!j&&prepared==1&&!written[RCP_CPCR]);
  free(a.TxDesc);free(a.RxDesc);for(i=0;i<RCP_NUM_TX_DESC;i++)free(a.CpTxBuf[i]);for(i=0;i<RCP_NUM_RX_DESC;i++)free(a.CpRxBuf[i]);
  clear();memset(&b,0,sizeof(b));b.DeviceId=k?0x8127:0x8169;b.TxDescPhys=0x10000;b.RxDescPhys=0x20000;
  b.TxDesc=calloc(1,CP_TX_RING_BYTES);rx=b.RxDesc=calloc(1,CP_RX_RING_BYTES);
  for(i=0;i<RCP_NUM_TX_DESC;i++)b.CpTxP[i]=0x30000+i*0x1000;
  for(i=0;i<RCP_NUM_RX_DESC;i++)b.CpRxP[i]=0x50000+i*0x1000;
  assert(cplus_init(&b)==!j);assert(enabled==!j);free(b.TxDesc);free(b.RxDesc);
 }
 absent=0;
 clear();prepare_fail=1;memset(&a,0,sizeof(a));assert(!CpInit(&a)&&!enabled&&!allocs&&prepared==1);prepare_fail=0;
 for(j=1;j<=2+RCP_NUM_TX_DESC+RCP_NUM_RX_DESC;j++){
  clear();fail=j;memset(&a,0,sizeof(a));assert(!CpInit(&a)&&!enabled);
  free(a.TxDesc);free(a.RxDesc);for(i=0;i<RCP_NUM_TX_DESC;i++)free(a.CpTxBuf[i]);for(i=0;i<RCP_NUM_RX_DESC;i++)free(a.CpRxBuf[i]);
 }
 legacy39=1;
 for(j=0;j<2;j++){
  clear();absent=j;fail=0;memset(&a,0,sizeof(a));
  assert(R39Init(&a)==!j&&enabled==!j);free(a.RxRing);for(i=0;i<4;i++)free(a.TxBuf[i]);
  clear();memset(&b,0,sizeof(b));b.RxRingPhys=0x10000;for(i=0;i<4;i++)b.TxPhys[i]=0x20000+0x10000*i;
  assert(r8139_init(&b)==!j&&enabled==!j);
 }
 absent=0;
 for(j=1;j<=5;j++){
  clear();fail=j;memset(&a,0,sizeof(a));assert(!R39Init(&a)&&!enabled);
  free(a.RxRing);for(i=0;i<4;i++)free(a.TxBuf[i]);
 }
 /* Some hardware retains the programmed station address across MAC reset.
    Permanent OID identity must not turn into the configured local address. */
 for(k=0;k<2;k++){
  const UCHAR factory[6]={0x00,0xe0,0x4c,0x12,0x34,0x56};
  const UCHAR configured[6]={0x02,0x11,0x22,0x33,0x44,0x55};
  memset(&a,0,sizeof(a));memcpy(a.CurrentAddr,configured,6);legacy39=k;fail=0;absent=0;
  for(j=0;j<20;j++){
   clear();for(i=0;i<6;i++)regs[i]=(j?configured:factory)[i];
   assert(k?R39Init(&a):CpInit(&a));
   assert(!memcmp(a.PermanentAddr,factory,6)&&!memcmp(a.CurrentAddr,configured,6));
   free(a.TxDesc);free(a.RxDesc);a.TxDesc=a.RxDesc=NULL;
   for(i=0;i<RCP_NUM_TX_DESC;i++){free(a.CpTxBuf[i]);a.CpTxBuf[i]=NULL;}
   for(i=0;i<RCP_NUM_RX_DESC;i++){free(a.CpRxBuf[i]);a.CpRxBuf[i]=NULL;}
   free(a.RxRing);a.RxRing=NULL;for(i=0;i<4;i++){free(a.TxBuf[i]);a.TxBuf[i]=NULL;}
  }
 }
 puts("PASS: NT3.1 and miniport C+/8139 initialize complete DMA rings before enable; removal and every allocation failure never enable DMA; 40 configured-MAC starts/resets preserve permanent identity");return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d)/'test.c';exe=Path(d)/'test'
    p.write_text(prefix+function('backend_cplus.c','CpInit')+function('drvnt31.c','cplus_init')+function('backend_8139.c','R39Init')+function('drvnt31.c','r8139_init')+main)
    subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I',str(src),str(p),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
