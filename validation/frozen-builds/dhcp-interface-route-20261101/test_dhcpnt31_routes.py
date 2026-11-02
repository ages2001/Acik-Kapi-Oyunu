"""Production DHCP route choice and per-adapter automatic opt-in checks."""
from pathlib import Path
import re, subprocess, tempfile
source = (Path(__file__).resolve().parents[1]/'RLTKLANT/DHCPNT31/src/dhcpnt31.c').read_text()
def function(name):
    m = re.search(r'static [^;{}]+\b'+name+r'\([^;{}]*\)\s*\{', source); assert m
    end, depth = m.end(), 1
    while depth:
        depth += (source[end]=='{')-(source[end]=='}'); end += 1
    # The Windows target is LLP64/ILP32: unsigned long is exactly 32 bits.
    return source[m.start():end].replace('unsigned long', 'uint32_t')
prefix = r'''
#include <stdint.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct {uint32_t ip,mask;} DHCP_ROUTE;
typedef void* HKEY; typedef uint32_t DWORD; typedef int LONG; typedef unsigned char* LPBYTE;
#define HKEY_LOCAL_MACHINE ((HKEY)1)
#define KEY_READ 1
#define REG_DWORD 4
#define ERROR_SUCCESS 0
#define ERROR_FILE_NOT_FOUND 2
#define SVCBASE "SYSTEM\\CurrentControlSet\\Services"
static int open_fail,query_status,closed;static DWORD stored=1,kind=4,bytes=4;
static void app_s(char*b,int*p,const char*s){while(*s)b[(*p)++]=*s++;}
static HKEY open_tcpip_n(int idx,char*svc){assert(idx==1);strcpy(svc,"RLTKLANT02");return (HKEY)2;}
static int RegCloseKey(HKEY h){assert(h==(HKEY)2||h==(HKEY)3);closed++;return 0;}
static int RegOpenKeyExA(HKEY h,const char*path,DWORD z,DWORD access,HKEY*out){assert(h==HKEY_LOCAL_MACHINE&&!z&&access==KEY_READ);assert(!strcmp(path,SVCBASE "\\RLTKLANT02\\Parameters"));if(open_fail)return 5;*out=(HKEY)3;return 0;}
static int RegQueryValueExA(HKEY h,const char*n,DWORD z,DWORD*t,LPBYTE data,DWORD*s){assert(h==(HKEY)3&&!z&&!strcmp(n,"EnableDHCP")&&*s==4);*t=kind;*s=bytes;memcpy(data,&stored,4);return query_status;}
'''
main = r'''
static DHCP_ROUTE route(const char*ip,const char*mask){DHCP_ROUTE r={inet_addr(ip),inet_addr(mask)};return r;}
int main(void){DHCP_ROUTE r[3];uint32_t dest=0;int previous;
 r[0]=route("10.0.2.15","255.255.255.0");r[1]=route("10.0.3.15","255.255.255.0");r[2]=route("10.0.4.15","255.255.255.0");
 assert(select_destination(r,1,0,&dest)&&dest==INADDR_BROADCAST);
 assert(select_destination(r,3,0,&dest)&&dest==inet_addr("10.0.2.255"));
 assert(select_destination(r,3,1,&dest)&&dest==inet_addr("10.0.3.255"));
 assert(select_destination(r,3,2,&dest)&&dest==inet_addr("10.0.4.255"));
 assert(!select_destination(r,0,0,&dest)&&!select_destination(r,65,0,&dest));
 assert(!select_destination(r,3,-1,&dest)&&!select_destination(r,3,3,&dest));
 r[1]=route("10.0.2.16","255.255.255.0");assert(!select_destination(r,3,0,&dest));
 r[1]=route("10.0.3.15","255.255.0.0");assert(!select_destination(r,3,0,&dest));
 r[1]=route("10.0.3.15","255.0.255.0");assert(!select_destination(r,3,1,&dest));
 r[1]=route("0.0.0.0","255.255.255.0");assert(!select_destination(r,3,1,&dest));
 r[1]=route("127.0.0.1","255.255.255.0");assert(!select_destination(r,3,1,&dest));
 r[1]=route("224.0.0.1","255.255.255.0");assert(!select_destination(r,3,1,&dest));
 r[1]=route("10.0.3.0","255.255.255.0");assert(!select_destination(r,3,1,&dest));
 r[1]=route("10.0.3.255","255.255.255.0");assert(!select_destination(r,3,1,&dest));
 r[1]=route("10.0.3.15","255.255.255.254");assert(!select_destination(r,3,1,&dest));
 r[1]=route("10.0.3.15","0.0.0.0");assert(!select_destination(r,3,1,&dest));
 assert(auto_enabled(1)==1&&closed==2);stored=0;assert(auto_enabled(1)==0);
 stored=2;assert(auto_enabled(1)==-1);stored=1;kind=1;assert(auto_enabled(1)==-1);kind=4;
 bytes=8;assert(auto_enabled(1)==-1);bytes=4;query_status=ERROR_FILE_NOT_FOUND;assert(auto_enabled(1)==0);
 query_status=5;assert(auto_enabled(1)==-1);query_status=0;open_fail=1;previous=closed;assert(auto_enabled(1)==-1&&closed==previous+1);
 puts("PASS: distinct subnet destinations, overlap/unconfigured/invalid rejection and per-adapter opt-in/type/error handling");return 0;}
'''
with tempfile.TemporaryDirectory(prefix='rltk-dhcp-routes-') as td:
    code, exe = Path(td)/'test.c', Path(td)/'test'
    code.write_text(prefix+function('select_destination')+function('auto_enabled')+main)
    subprocess.run(['gcc','-std=c99','-g','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-fno-omit-frame-pointer',str(code),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
