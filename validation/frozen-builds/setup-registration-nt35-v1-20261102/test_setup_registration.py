"""Fault-inject the production NT3.1 installer-registration transaction."""
from pathlib import Path
import re, subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'RLTKLANT/tools/rltsetup.c').read_text()
a=source.index('#ifndef SETUP_INF');b=source.index('static BOOL run_setup',a)
production=source[a:b]
prefix=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
typedef unsigned DWORD;typedef long LONG;typedef unsigned char BYTE;typedef int HKEY;typedef int FILETIME;
#define MAX_PATH 260
#define CARDS "cards"
#define HKEY_LOCAL_MACHINE 77
#define KEY_READ 1
#define KEY_QUERY_VALUE 2
#define KEY_SET_VALUE 4
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_INVALID_DATA 13
#define ERROR_NO_MORE_ITEMS 259
#define REG_SZ 1
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define FILE_ATTRIBUTE_DIRECTORY 16
#define lstrcatA strcat
#define lstrlenA strlen
#define lstrcmpiA strcasecmp
static const char *failure;
static int fail(const char*s){failure=s;return 0;}
static char values[3][MAX_PATH];static int exists[3],opened,closed,op,fail_at,failed,write_count,missing_inf,fail_write,fail_rollback;
static int inject(void){op++;if(op==fail_at&&!failed){failed=1;return 5;}return 0;}
static int slot(HKEY k){return k==1000?0:k==1002?1:k==1500?2:-1;}
static DWORD GetSystemDirectoryA(char*p,DWORD n){(void)n;strcpy(p,"C:\\WINNT31\\system32");return strlen(p);}
static DWORD GetFileAttributesA(const char*p){assert(!strcmp(p,"C:\\WINNT31\\system32\\RLTKNT31.INF"));return missing_inf?INVALID_FILE_ATTRIBUTES:0;}
static LONG RegOpenKeyExA(HKEY root,const char*name,DWORD zero,DWORD rights,HKEY*out){int r=inject();(void)zero;(void)rights;if(r)return r;
 if(root==77)*out=!strcmp(name,"cards")?1:500;
 else if(root==1)*out=100+(name[0]-'0');
 else {assert(!strcmp(name,"NetRules"));*out=root+900+(root==500?100:0);}
 opened++;return 0;}
static LONG RegCloseKey(HKEY k){assert(k);closed++;return 0;}
static LONG RegEnumKeyExA(HKEY k,DWORD i,char*p,DWORD*n,void*a,void*b,void*c,FILETIME*d){int r=inject();(void)n;(void)a;(void)b;(void)c;(void)d;assert(k==1);if(r)return r;if(i==3)return ERROR_NO_MORE_ITEMS;p[0]='0'+i;p[1]=0;return 0;}
static LONG text_value(HKEY k,const char*name,char*p,DWORD n){int r=inject();(void)n;if(r)return r;
 if(!strcmp(name,"ProductName")){assert(k>=100&&k<=102);strcpy(p,k==101?"Unrelated":"RLTKLANT");return 0;}
 {int i=slot(k);assert(i>=0);if(!exists[i])return ERROR_FILE_NOT_FOUND;strcpy(p,values[i]);return 0;}}
static LONG RegSetValueExA(HKEY k,const char*n,DWORD zero,DWORD kind,const BYTE*data,DWORD size){int i=slot(k),r=inject();(void)zero;assert(i>=0&&!strcmp(n,"InfName")&&kind==REG_SZ&&size==strlen((const char*)data)+1);if(r)return r;
 write_count++;if(write_count==fail_write)return 5;
 if(fail_rollback&&strcmp((const char*)data,"RLTKNT31.INF"))return 5;
 strcpy(values[i],(const char*)data);exists[i]=1;return 0;}
static LONG RegDeleteValueA(HKEY k,const char*n){int i=slot(k);assert(i>=0&&!strcmp(n,"InfName"));if(fail_rollback)return 5;exists[i]=0;return 0;}
'''
tests=r'''
static void reset(void){int i;opened=closed=op=failed=write_count=missing_inf=fail_at=fail_write=fail_rollback=0;failure=0;for(i=0;i<3;i++){exists[i]=1;sprintf(values[i],"old%d.inf",i);}}
static void restored(void){int i;for(i=0;i<3;i++){char x[20];sprintf(x,"old%d.inf",i);assert(exists[i]&&!strcmp(values[i],x));}assert(opened==closed);}
int main(void){int i,total;
 reset();assert(refresh_registration());assert(opened==closed);for(i=0;i<3;i++)assert(!strcmp(values[i],"RLTKNT31.INF"));total=op;
 for(i=1;i<=total;i++){reset();fail_at=i;assert(!refresh_registration());assert(failure);restored();}
 reset();missing_inf=1;assert(!refresh_registration());restored();assert(!opened);
 for(i=1;i<=3;i++){reset();fail_write=i;assert(!refresh_registration());restored();}
 reset();exists[0]=0;fail_write=2;assert(!refresh_registration());assert(!exists[0]&&opened==closed);
 reset();fail_write=3;fail_rollback=1;assert(!refresh_registration());assert(strstr(failure,"rollback incomplete"));assert(opened==closed);
 reset();assert(refresh_registration());assert(refresh_registration());assert(opened==closed);
 printf("PASS: %d registry-operation faults, each write failure, absent values, rollback-failure reporting, unrelated product exclusion, repeat and handle cleanup\n",total);
}
'''
with tempfile.TemporaryDirectory(prefix='rltk-setup-registration-') as td:
 p=Path(td)/'test.c';p.write_text(prefix+production+tests);exe=Path(td)/'test'
 subprocess.run(['gcc','-std=c99','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
