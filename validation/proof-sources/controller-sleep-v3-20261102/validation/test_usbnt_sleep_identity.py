"""Actual descriptor comparison and root-PDO relations while the HC is asleep."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
def fn(source,name):
    m=re.search(r'(?:static )?(?:int|void|NTSTATUS) '+name+r'\([^;{]*\)\s*\{',source);assert m,name
    end=m.end();depth=1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]
core=(root/'USBNT/src/usbcore.c').read_text()
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t UCHAR;typedef uint16_t USHORT;typedef uint32_t ULONG;
#define USB_OK 0
#define USB_ERR_IO -1
#define USB_ERR_NODEV -2
#define USB_DT_STRING 3
#define GET16(p) ((USHORT)((p)[0]|((p)[1]<<8)))
typedef struct {UCHAR DevDesc[18];char Mfg[32],Product[48],Serial[32];} USB_DEV;
static int call,fail,change,bad,checks;
static const char*strings[]={"Maker","Product","Serial-1"};
static int UsbGetDescriptor(USB_DEV*d,UCHAR type,UCHAR index,USHORT lang,void*buf,USHORT size,ULONG*got){UCHAR*b=buf;int i,n;(void)d;assert(type==3&&size>=32);if(++call==fail)return USB_ERR_IO;
 if(!index){assert(!lang);b[0]=6;b[1]=3;b[2]=7;b[3]=4;b[4]=9;b[5]=4;*got=6;}
 else{assert(lang==0x409&&index<=3);n=strlen(strings[index-1]);b[0]=2+n*2;b[1]=3;for(i=0;i<n;i++){b[2+i*2]=strings[index-1][i];b[3+i*2]=0;}*got=b[0];if(change==index)b[2]='X';}
 if(bad==call)b[1]=0;return 0;}
'''
main=r'''
int main(void){USB_DEV d,old;int i;memset(&d,0,sizeof(d));d.DevDesc[14]=1;d.DevDesc[15]=2;d.DevDesc[16]=3;strcpy(d.Mfg,strings[0]);strcpy(d.Product,strings[1]);strcpy(d.Serial,strings[2]);old=d;
 for(i=0;i<5;i++){call=0;fail=i;assert(UsbCheckDeviceStrings(&d)==(i?USB_ERR_IO:0));assert(!memcmp(&d,&old,sizeof(d)));checks++;}fail=0;
 for(i=1;i<4;i++){call=0;change=i;assert(UsbCheckDeviceStrings(&d)==USB_ERR_NODEV);assert(!memcmp(&d,&old,sizeof(d)));checks++;}change=0;
 for(i=1;i<5;i++){call=0;bad=i;assert(UsbCheckDeviceStrings(&d)==USB_ERR_IO);assert(!memcmp(&d,&old,sizeof(d)));checks++;}bad=0;
 memset(&d,0,sizeof(d));call=0;assert(!UsbCheckDeviceStrings(&d)&&!call);checks++;
 printf("PASS: %d actual identity checks; transport errors retain identity, malformed descriptors fail, changed strings request PnP, cached bytes preserved\n",checks);return 0;}
'''
tree=(root/'USBNT/src/wdmtree.c').read_text()
tree_prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
typedef int NTSTATUS,KIRQL;
#define TRUE 1
#define FALSE 0
#define STATUS_SUCCESS 0
#define PDO_HUB 3
typedef struct {int dummy;} IRP,*PIRP;
typedef struct {int Removed,Running,Sleeping;} USB_HC;
typedef struct W {struct W*TNext;void*TOwner,*TDev;struct {void*Self;} Nt;int Bus,Started,Gone,Reported;} WDM_EXT;
static WDM_EXT *Nodes,created,*merged;static USB_HC hc,*selected;static int TreeLock,news,preinstalls,merges;
static void Init(void){}
static USB_HC*HcOf(void*p,USB_HC*out){assert(p);if(!selected)return NULL;*out=*selected;return out;}
static void KeAcquireSpinLock(int*l,int*i){assert(l==&TreeLock);*i=0;}
static void KeReleaseSpinLock(int*l,int i){assert(l==&TreeLock&&!i);}
static WDM_EXT*NewNode(int k,void*p,void*a,void*b){assert(k==3&&p&&!a&&!b);news++;return &created;}
static void Preinstall(WDM_EXT*x){assert(x==&created);preinstalls++;}
static int Merge(PIRP p,WDM_EXT**x,int n){assert(p&&n==1);merged=*x;merges++;return 0;}
'''
tree_main=r'''
int main(void){int sleeping,running,removed,started,checks=0;IRP irp;WDM_EXT parent={0},root={0};parent.Nt.Self=&parent;parent.Bus=2;root.TOwner=&parent;Nodes=&root;
 for(sleeping=0;sleeping<2;sleeping++)for(running=0;running<2;running++)for(removed=0;removed<2;removed++)for(started=0;started<2;started++){
 hc.Sleeping=sleeping;hc.Running=running;hc.Removed=removed;parent.Started=started;selected=&hc;root.Reported=1;merges=0;merged=NULL;
 assert(!WdmTreeHcRelations(&parent,&irp));int present=!removed&&started&&(running||sleeping);assert(root.Reported==present&&merges==present);assert(!news&&!preinstalls);if(present)assert(merged==&root);checks++;}
 selected=NULL;assert(!WdmTreeHcRelations(&parent,&irp)&&!root.Reported&&!news);checks++;
 printf("PASS: %d actual root-PDO relation states; retained sleep never withdraws or replaces the root object\n",checks);return 0;}
'''
with tempfile.TemporaryDirectory() as tmp:
    for name,source in [('strings',prefix+fn(core,'ReadString')+fn(core,'UsbCheckDeviceStrings')+main),('relations',tree_prefix+fn(tree,'WdmTreeHcRelations')+tree_main)]:
        p=Path(tmp)/(name+'.c');exe=Path(tmp)/name;p.write_text(source)
        subprocess.run(['gcc','-std=c99','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
