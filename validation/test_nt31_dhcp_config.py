"""Production NT3.1 per-instance MAC publication and global resolver updates."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];driver=(root/'RLTKLANT/src/drvnt31.c').read_text();dhcp=(root/'RLTKLANT/DHCPNT31/src/dhcpnt31.c').read_text()
def fn(source,name):
 m=re.search(r'static \w+\s+'+name+r'\([^;{]*\)\s*\{',source);assert m,name
 end=m.end();depth=1
 while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
 return source[m.start():end]
mac=r'''
#define __stdcall
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ndisnt31.h"
typedef struct {UCHAR Mac[6];} Adapter,*PN31_ADAPTER;
typedef struct {ULONG Length;PVOID RootDirectory;PNDIS_STRING ObjectName;ULONG Attributes;PVOID SecurityDescriptor,SecurityQoS;} OBJ_ATTR;
static char path[256],value[64];static int opened,written,closed,good,openfail,setfail;
static void dbg(const char*s){if(!strcmp(s,"MAC published for adapter instance\n"))good++;}
NDIS_STATUS ZwOpenKey(PVOID*h,ULONG access,OBJ_ATTR*o){unsigned i;assert(access==2);opened++;for(i=0;i<o->ObjectName->Length/2;i++)path[i]=(char)o->ObjectName->Buffer[i];path[i]=0;if(openfail)return -1;*h=(PVOID)1;return 0;}
NDIS_STATUS ZwSetValueKey(PVOID h,PNDIS_STRING key,ULONG title,ULONG type,PVOID data,ULONG size){unsigned i;(void)title;assert(h==(PVOID)1&&type==1&&size==36&&key->Length==26);written++;for(i=0;i<size/2;i++)value[i]=(char)((WCHAR*)data)[i];return setfail?-1:0;}
NDIS_STATUS ZwClose(PVOID h){assert(h==(PVOID)1);closed++;return 0;}
'''
macmain=r'''
int main(void){Adapter a={{0x52,0x54,0,0xab,0xcd,0xef}};WCHAR dev[]=L"\\Device\\RLTKLANT01",other[]=L"RLTKLANT02";NDIS_STRING n={sizeof(dev)-2,sizeof(dev),dev};
 publish_mac(&a,&n);assert(opened==1&&written==1&&closed==1&&good==1);
 assert(!strcmp(path,"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\RLTKLANT01\\Parameters")&&!strcmp(value,"52:54:00:ab:cd:ef"));
 n.Buffer=other;n.Length=sizeof(other)-2;publish_mac(&a,&n);assert(strstr(path,"RLTKLANT02\\Parameters")&&good==2);
 n.Length=3;publish_mac(&a,&n);assert(opened==2);publish_mac(&a,NULL);assert(opened==2);
 n.Length=sizeof(other)-2;openfail=1;publish_mac(&a,&n);assert(good==2&&written==2&&closed==2);openfail=0;setfail=1;publish_mac(&a,&n);assert(good==2&&closed==3);
 puts("PASS: NT3.1 per-instance MAC publication, two adapter names, malformed length, registry open/write failure reporting");return 0;}
'''
dns=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef void*HKEY;typedef int LONG;typedef unsigned DWORD;typedef unsigned char BYTE,*LPBYTE;
#define HKEY_LOCAL_MACHINE ((HKEY)1)
#define KEY_SET_VALUE 2
#define KEY_QUERY_VALUE 1
#define REG_DWORD 4
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_INVALID_DATA 13
#define ERROR_SUCCESS 0
#define REG_SZ 1
#define SVCBASE "SYSTEM\\CurrentControlSet\\Services"
#define my_strlen strlen
static char applied_dns[512],stored[512];static int writes,opens,closes,openfail,setfail,messages;static DWORD lookup=4;static int lookupwrites;
static void out(const char*s){(void)s;messages++;}
LONG RegOpenKeyExA(HKEY root,const char*key,DWORD options,DWORD access,HKEY*out){assert(root==HKEY_LOCAL_MACHINE&&!options&&access==(KEY_SET_VALUE|KEY_QUERY_VALUE)&&!strcmp(key,SVCBASE "\\Tcpip\\Parameters"));opens++;if(openfail)return 5;*out=(HKEY)2;return 0;}
LONG RegQueryValueExA(HKEY h,const char*name,DWORD z,DWORD*t,LPBYTE b,DWORD*n){assert(h==(HKEY)2&&!strcmp(name,"DNSLookupOrder")&&!z&&*n==4);*t=REG_DWORD;memcpy(b,&lookup,4);return 0;}
LONG RegSetValueExA(HKEY h,const char*key,DWORD zero,DWORD type,const BYTE*data,DWORD size){assert(h==(HKEY)2&&!zero);if(!strcmp(key,"DNSLookupOrder")){assert(type==REG_DWORD&&size==4);memcpy(&lookup,data,4);lookupwrites++;return 0;}assert(!strcmp(key,"NameServer")&&type==REG_SZ&&size==strlen((const char*)data)+1);writes++;if(setfail)return 5;strcpy(stored,(const char*)data);return 0;}
LONG RegCloseKey(HKEY h){assert(h==(HKEY)2);closes++;return 0;}
'''
dnsmain=r'''
int main(void){char huge[512];
 assert(!apply_dns("")&&!opens);assert(!apply_dns("10.0.2.3")&&!strcmp(stored,"10.0.2.3")&&writes==1);
 assert(!apply_dns("10.0.2.3")&&writes==1);assert(!apply_dns("10.0.3.3")&&!strcmp(stored,"10.0.2.3 10.0.3.3")&&writes==2);
 openfail=1;assert(apply_dns("10.0.4.3")==1&&writes==2&&!strcmp(applied_dns,stored));openfail=0;
 setfail=1;assert(apply_dns("10.0.4.3")==1&&writes==3&&!strcmp(applied_dns,stored));setfail=0;
 assert(!apply_dns("10.0.4.3")&&!strcmp(stored,"10.0.2.3 10.0.3.3 10.0.4.3"));
 memset(huge,'x',sizeof(huge)-1);huge[sizeof(huge)-1]=0;assert(apply_dns(huge)==1&&!strcmp(applied_dns,stored));
 assert(closes==4&&messages==3&&lookup==1&&lookupwrites==1);
 lookup=3;assert(!apply_dns("10.0.5.3")&&lookup==3&&lookupwrites==1);
 puts("PASS: NT3.1 global DNS path, disabled-DNS enablement, enabled-order preservation, ordered unique servers, empty/bounded list and registry failure rollback");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 for name,code in [('mac',mac+fn(driver,'publish_mac')+macmain),('dns',dns+fn(dhcp,'set_sz')+fn(dhcp,'apply_dns')+dnsmain)]:
  p=Path(d)/(name+'.c');exe=Path(d)/name;p.write_text(code)
  subprocess.run(['gcc','-std=c99','-g','-fshort-wchar','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(root/'RLTKLANT/src'),str(p),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
