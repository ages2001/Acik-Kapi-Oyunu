"""Fault-inject the production NT3.1 counted CF_TEXT reply reader."""
from pathlib import Path
import re, subprocess, tempfile

source = (Path(__file__).resolve().parents[1] / 'RLTKLANT/tools/rltsetup.c').read_text()
start = source.index('static int request(')
finish = source.index('\nstatic int execute(', start)
function = source[start:finish]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint32_t DWORD;typedef unsigned char BYTE;typedef int HSZ;typedef int HDDEDATA;
#define CP_WINANSI 1004
#define CF_TEXT 1
#define XTYP_REQUEST 0x20b0
static char data_buffer[65536];static DWORD instance=12;static int conversation=42;
static int allocation_failure,request_failure,freed_item,freed_data,copies;static DWORD advertised,copied;
static BYTE source_data[65536];
static int fail(const char*s){assert(s);return 0;}
static HSZ DdeCreateStringHandleA(DWORD i,const char*n,int cp){assert(i==12&&!strcmp(n,"Startup")&&cp==CP_WINANSI);return allocation_failure?0:11;}
static HDDEDATA DdeClientTransaction(BYTE*b,DWORD size,int c,HSZ h,int f,int t,int wait,void*r){assert(!b&&!size&&c==42&&h==11&&f==CF_TEXT&&t==XTYP_REQUEST&&wait==5000&&!r);return request_failure?0:17;}
static void DdeFreeStringHandle(DWORD i,HSZ h){assert(i==12&&h==11);freed_item++;}
static void DdeFreeDataHandle(HDDEDATA h){assert(h==17);freed_data++;}
static DWORD DdeGetData(HDDEDATA h,BYTE*out,DWORD size,DWORD offset){assert(h==17&&!offset);if(!out){assert(!size);return advertised;}assert(size==advertised&&size<sizeof(data_buffer));memcpy(out,source_data,size);copies++;return copied;}
'''
tests = r'''
static void reset(void){allocation_failure=request_failure=freed_item=freed_data=copies=0;memset(data_buffer,0xa5,sizeof(data_buffer));memset(source_data,0,sizeof(source_data));advertised=copied=5;memcpy(source_data,"abc\r\n",5);}
int main(void){
 reset();assert(request("Startup")&&!strcmp(data_buffer,"abc\r\n")&&data_buffer[6]==(char)0xa5&&copies==1&&freed_item==1&&freed_data==1);
 reset();advertised=copied=6;assert(request("Startup")&&!strcmp(data_buffer,"abc\r\n"));
 reset();advertised=copied=16;assert(request("Startup")&&!strcmp(data_buffer,"abc\r\n"));
 reset();source_data[1]=0;assert(request("Startup")&&!strcmp(data_buffer,"a")&&freed_data==1);
 reset();copied=4;assert(!request("Startup")&&freed_data==1);
 reset();advertised=0;assert(!request("Startup")&&!copies&&freed_data==1);
 reset();advertised=sizeof(data_buffer);assert(!request("Startup")&&!copies&&freed_data==1);
 reset();allocation_failure=1;assert(!request("Startup")&&!freed_item&&!freed_data);
 reset();request_failure=1;assert(!request("Startup")&&freed_item==1&&!freed_data);
 puts("PASS: counted CF_TEXT with/without NUL, zero padding, bounded first-NUL text termination, length mismatch, capacity and DDE failure cleanup");return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-dde-reply-') as directory:
    code, executable = Path(directory)/'test.c', Path(directory)/'test'
    code.write_text(prefix + function + tests)
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined',
                    str(code), '-o', str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
