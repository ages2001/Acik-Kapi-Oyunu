/* Disposable NT3.51 NDIS protocol diagnostic; never a release driver.
 * Register/open using public NDIS APIs, then dispatch the NDIS registered
 * MAC Request/Reset callbacks through the opened binding. This includes
 * the NDIS miniport wrapper, not a direct miniport handler call. NT3.51 has
 * no NdisReset/NdisRequest exports and no NT4 cached reset-handler layout.
 * Its actual NdisOpenAdapter and NdisRegisterMac implementations corroborate
 * the prefix and 0x30/0x34 callback offsets; see nt351-protocol-abi-20261102.
 * No NDIS patches. Small resident test image is retained until reboot.
 */
#include "ndisnt31.h"
#include "nt31ndisabi.h"
VOID STDC NdisRegisterProtocol(PNDIS_STATUS,PNDIS_HANDLE,PVOID,UINT);
VOID STDC NdisDeregisterProtocol(PNDIS_STATUS,NDIS_HANDLE);
VOID STDC NdisOpenAdapter(PNDIS_STATUS,PNDIS_STATUS,PNDIS_HANDLE,PUINT,
    NDIS_MEDIUM*,UINT,NDIS_HANDLE,NDIS_HANDLE,PNDIS_STRING,UINT,PVOID);
VOID STDC NdisCloseAdapter(PNDIS_STATUS,NDIS_HANDLE);
typedef struct {
    PVOID AdapterQueue,Context;NDIS_SPIN_LOCK ReferenceLock;
    USHORT References;BOOLEAN Closing;UCHAR Pad;UINT Length;
    NDIS_MAC_CHARACTERISTICS Characteristics;
} MAC_PREFIX;
typedef char CheckReset[__builtin_offsetof(MAC_PREFIX,Characteristics.ResetHandler)==0x30?1:-1];
typedef char CheckRequest[__builtin_offsetof(MAC_PREFIX,Characteristics.RequestHandler)==0x34?1:-1];
typedef struct {ULONG Event[8];NDIS_STATUS Status;ULONG Resets,Starts,Ends;} CONTEXT;
static CONTEXT context;
static VOID trace(const char*s,ULONG n){int i;while(*s)WRITE_PORT_UCHAR((PUCHAR)0xe9,*s++);for(i=28;i>=0;i-=4)WRITE_PORT_UCHAR((PUCHAR)0xe9,"0123456789ABCDEF"[(n>>i)&15]);WRITE_PORT_UCHAR((PUCHAR)0xe9,'\n');}
static VOID zero(PVOID p,UINT n){volatile UCHAR*b=p;while(n--)*b++=0;}
static VOID STDC complete(NDIS_HANDLE c,NDIS_STATUS s){CONTEXT*b=c;b->Status=s;KeSetEvent(b->Event,0,FALSE);}
static VOID STDC opened(NDIS_HANDLE c,NDIS_STATUS s,NDIS_STATUS e){(void)e;complete(c,s);}
static VOID STDC reset_complete(NDIS_HANDLE c,NDIS_STATUS s){CONTEXT*b=c;b->Resets++;trace("N35TEST reset-complete=",s);complete(c,s);}
static VOID STDC requested(NDIS_HANDLE c,PNDIS_REQUEST r,NDIS_STATUS s){(void)r;complete(c,s);}
static VOID STDC sent(NDIS_HANDLE c,PVOID p,NDIS_STATUS s){(void)c;(void)p;(void)s;}
static VOID STDC transferred(NDIS_HANDLE c,PVOID p,NDIS_STATUS s,UINT n){(void)c;(void)p;(void)s;(void)n;}
static NDIS_STATUS STDC received(NDIS_HANDLE c,NDIS_HANDLE r,PVOID h,UINT hn,PVOID l,UINT ln,UINT n){(void)c;(void)r;(void)h;(void)hn;(void)l;(void)ln;(void)n;return 0x10003;}
static VOID STDC notified(NDIS_HANDLE c){(void)c;}
static VOID STDC status(NDIS_HANDLE c,NDIS_STATUS s,PVOID p,UINT n){CONTEXT*b=c;(void)p;(void)n;if(s==0x40010004)b->Starts++;if(s==0x40010005)b->Ends++;trace("N35TEST status=",s);}
static NDIS_STATUS wait_status(NDIS_STATUS s){if(s==NDIS_STATUS_PENDING){KeWaitForSingleObject(context.Event,0,0,FALSE,NULL);s=context.Status;}return s;}
static NTSTATUS STDC run(PVOID unused)
{
    N31_PROTOCOL_CHARACTERISTICS c;NDIS_HANDLE protocol=NULL,adapter=NULL;
    NDIS_STATUS s,error;NDIS_MEDIUM medium=NdisMedium802_3;UINT selected,i;
    NDIS_STRING name;NDIS_REQUEST query;ULONG vendor=0;UCHAR permanent[6];N31_OPEN_PREFIX*open;MAC_PREFIX*mac;
    WCHAR pname[]=L"RltkNt351ResetTest",aname[]=L"\\Device\\RLTKLANT2";
    (void)unused;zero(&c,sizeof(c));KeInitializeEvent(context.Event,0,FALSE);
    c.MajorVersion=3;c.OpenComplete=opened;c.CloseComplete=complete;c.SendComplete=sent;
    c.TransferComplete=transferred;c.ResetComplete=reset_complete;c.RequestComplete=requested;
    c.Receive=received;c.ReceiveComplete=notified;c.Status=status;c.StatusComplete=notified;
    c.Name.Buffer=pname;c.Name.Length=sizeof(pname)-2;c.Name.MaximumLength=sizeof(pname);
    NdisRegisterProtocol(&s,&protocol,&c,sizeof(c));trace("N35TEST register=",s);if(s)return 0;
    name.Buffer=aname;name.Length=sizeof(aname)-2;name.MaximumLength=sizeof(aname);
    NdisOpenAdapter(&s,&error,&adapter,&selected,&medium,1,protocol,&context,&name,0,NULL);
    s=wait_status(s);trace("N35TEST open=",s);if(s)goto deregister;
    open=adapter;mac=open->MacHandle;
    zero(&query,sizeof(query));query.RequestType=NdisRequestQueryInformation;
    query.DATA.QUERY_INFORMATION.Oid=OID_GEN_VENDOR_ID;query.DATA.QUERY_INFORMATION.InformationBuffer=&vendor;
    query.DATA.QUERY_INFORMATION.InformationBufferLength=sizeof(vendor);
    KeResetEvent(context.Event);s=mac->Characteristics.RequestHandler(open->MacBindingHandle,&query);
    s=wait_status(s);trace("N35TEST query=",s);trace("N35TEST vendor=",vendor);
    zero(permanent,sizeof(permanent));
    if(!s){
        query.DATA.QUERY_INFORMATION.Oid=OID_802_3_PERMANENT_ADDRESS;
        query.DATA.QUERY_INFORMATION.InformationBuffer=permanent;
        query.DATA.QUERY_INFORMATION.InformationBufferLength=sizeof(permanent);
        KeResetEvent(context.Event);s=mac->Characteristics.RequestHandler(open->MacBindingHandle,&query);
        s=wait_status(s);trace("N35TEST permanent-query=",s);
    }
    /* Check returned bytes independently of the driver's implementation. */
    if(!s&&((PUCHAR)&vendor)[0]==permanent[0]&&((PUCHAR)&vendor)[1]==permanent[1]&&
       ((PUCHAR)&vendor)[2]==permanent[2]&&((PUCHAR)&vendor)[3]==0){
        for(i=0;i<3;i++){
            KeResetEvent(context.Event);s=mac->Characteristics.ResetHandler(open->MacBindingHandle);
            trace("N35TEST reset-return=",s);if(s!=NDIS_STATUS_PENDING)break;
            s=wait_status(s);if(s)break;
        }
    }
    KeResetEvent(context.Event);NdisCloseAdapter(&s,adapter);s=wait_status(s);trace("N35TEST close=",s);
    trace("N35TEST completions=",context.Resets);trace("N35TEST starts=",context.Starts);trace("N35TEST ends=",context.Ends);
    trace("N35TEST result=",(!s&&context.Resets==3&&context.Starts==3&&context.Ends==3)?0:1);
deregister:
    NdisDeregisterProtocol(&s,protocol);trace("N35TEST deregister=",s);return 0;
}
NTSTATUS STDC DriverEntry(PDRV_OBJ drv,PVOID registry)
{
    PUCHAR src=drv->DriverStart,dst;ULONG size=drv->DriverSize,pe,opt,rel,bytes,at,i,delta;
    NDIS_PHYSICAL_ADDRESS high={0xffffffff,0};(void)registry;
    dst=MmAllocateContiguousMemory((size+4095)&~4095,high);if(!dst)return (NTSTATUS)0xc000009a;
    for(i=0;i<size;i++)dst[i]=src[i];
    delta=(ULONG)dst-(ULONG)src;
    pe=*(ULONG*)(dst+0x3c);opt=pe+24;rel=*(ULONG*)(dst+opt+136);bytes=*(ULONG*)(dst+opt+140);
    for(at=0;rel&&at<bytes;){ULONG page=*(ULONG*)(dst+rel+at),n=*(ULONG*)(dst+rel+at+4),j;
        if(n<8||n>bytes-at)return (NTSTATUS)0xc0000001;
        for(j=8;j<n;j+=2){USHORT e=*(USHORT*)(dst+rel+at+j);if((e>>12)==3)*(ULONG*)(dst+page+(e&4095))+=delta;}at+=n;}
    return ((NTSTATUS(STDC*)(PVOID))(dst+((PUCHAR)run-src)))(NULL);
}
static PVOID image_anchor __attribute__((used))=(PVOID)DriverEntry;
