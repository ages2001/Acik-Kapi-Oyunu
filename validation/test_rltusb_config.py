"""Fault-inject the production legacy setup transaction and address parser."""
from pathlib import Path
import re, subprocess, tempfile

source=(Path(__file__).resolve().parents[1]/'RLTKLANT/tools/rltusb.c').read_text()
def function(name):
    match=re.search(r'^static [^\n]*\b'+name+r'\([^\n]*',source,re.M)
    assert match,name
    start=match.start();brace=source.index('{',start);depth=0
    for end in range(brace,len(source)):
        if source[end]=='{':depth+=1
        elif source[end]=='}':
            depth-=1
            if not depth:return source[start:end+1]+'\n'
    raise AssertionError(name)

prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t DWORD;typedef int32_t LONG;typedef unsigned char BYTE;
typedef void* HKEY;
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_MORE_DATA 234
#define REG_SZ 1
#define REG_DWORD 4
#define KEY_QUERY_VALUE 1
#define KEY_SET_VALUE 2
#define lstrlenA(s) ((int)strlen(s))
typedef struct { char service[64]; DWORD usb; char mac[32]; } CARD;
typedef struct { DWORD type, size; BYTE data[256]; int present; } VALUE;
static VALUE registry[3],before[3];static unsigned writes,fail_at,query_fail,queries,warnings;
static int index_of(const char*n){if(!strcmp(n,"UsbPermanentAddress"))return 0;if(!strcmp(n,"BusType"))return 1;assert(!strcmp(n,"UsbNtAdapter"));return 2;}
static LONG parameters(const char*n,unsigned access,HKEY*k){assert(!strcmp(n,"RLTKLANT1"));assert(access==3);*k=registry;return 0;}
static LONG RegQueryValueExA(HKEY k,const char*n,void*r,DWORD*t,BYTE*b,DWORD*s){VALUE*v=&registry[index_of(n)];assert(k==registry&&!r);if(++queries==query_fail)return 5;if(!v->present)return ERROR_FILE_NOT_FOUND;*t=v->type;if(*s<v->size){*s=v->size;return ERROR_MORE_DATA;}*s=v->size;memcpy(b,v->data,*s);return 0;}
static LONG RegSetValueExA(HKEY k,const char*n,unsigned reserved,DWORD t,const BYTE*b,DWORD s){VALUE*v=&registry[index_of(n)];assert(k==registry&&!reserved&&s<=sizeof(v->data));if(++writes==fail_at)return 5;memset(v,0,sizeof(*v));v->present=1;v->type=t;v->size=s;memcpy(v->data,b,s);return 0;}
static LONG RegDeleteValueA(HKEY k,const char*n){VALUE*v=&registry[index_of(n)];assert(k==registry);if(++writes==fail_at)return 5;if(!v->present)return ERROR_FILE_NOT_FOUND;memset(v,0,sizeof(*v));return 0;}
static void RegCloseKey(HKEY k){assert(k==registry);}
static void error(const char*s,LONG r){assert(s&&r);warnings++;}
'''
test=r'''
static void seed(unsigned bits){unsigned i,j;memset(registry,0,sizeof(registry));for(i=0;i<3;i++)if(bits&(1U<<i)){registry[i].present=1;registry[i].type=1+(bits+i)%11;registry[i].size=1+(bits*3+i)%200;for(j=0;j<registry[i].size;j++)registry[i].data[j]=(BYTE)(j*7+bits+i);}memcpy(before,registry,sizeof(before));writes=queries=warnings=fail_at=query_fail=0;}
int main(void){CARD card={"RLTKLANT1",0,""};unsigned i,f,mode,cases=0;
 assert(service_name("RLTKLANT1")&&service_name("rltklant123"));
 assert(!service_name("")&&!service_name("RLTKLANT")&&!service_name("RLTKLANT1\\Tcpip")&&!service_name("USBNT1"));
 assert(mac_address("525400abcdef")&&mac_address("02ABCDEF1234"));
 assert(!mac_address("")&&!mac_address("000000000000")&&!mac_address("ffffffffffff")&&!mac_address("535400abcdef")&&!mac_address("525400abcdeg")&&!mac_address("52:54:00:ab:cd:ef"));
 for(i=0;i<64;i++)for(mode=0;mode<2;mode++){
  for(f=1;f<=(mode?3U:2U);f++){seed(i);fail_at=f;assert(configure(&card,mode?"525400abcdef":NULL)==5);assert(!warnings);assert(!memcmp(registry,before,sizeof(before)));cases++;}
  for(f=1;f<=3;f++){seed(i);query_fail=f;assert(configure(&card,mode?"525400abcdef":NULL)==5);assert(!writes&&!warnings&&!memcmp(registry,before,sizeof(before)));cases++;}
  seed(i);assert(!configure(&card,mode?"525400abcdef":NULL));assert(registry[1].type==REG_DWORD&&registry[1].size==4&&*(DWORD*)registry[1].data==(mode?0U:5U));assert(registry[2].type==REG_DWORD&&registry[2].size==4&&*(DWORD*)registry[2].data==mode);if(mode)assert(registry[0].type==REG_SZ&&registry[0].size==13&&!strcmp((char*)registry[0].data,"525400abcdef"));else assert(!memcmp(&registry[0],&before[0],sizeof(VALUE)));cases++;
 }
 printf("PASS: %u production setup transaction schedules; original registry types/absence restored on failure; address and service validation\n",cases);return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='rltusb-test-') as td:
    td=Path(td);c=td/'test.c';binary=td/'test'
    c.write_text(prefix+''.join(function(x) for x in ('clear','service_name','hexchar','mac_address','snapshot','restore','configure'))+test)
    subprocess.run(['cc','-std=c11','-O1','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
