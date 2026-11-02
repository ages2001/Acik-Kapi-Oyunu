/* Test the linked adapter export, including structure byte order and bounds. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
static HANDLE logh;static DWORD checks,failures;
static void value(const char *s,DWORD n){char b[160],d[12];DWORD i=0,j=0,w;while(*s)b[i++]=*s++;do{d[j++]='0'+n%10;n/=10;}while(n);while(j)b[i++]=d[--j];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
void WINAPI entry(void){HMODULE dll;int(WINAPI *format)(REFGUID,LPOLESTR,int);GUID g={0x01234567,0x89ab,0xcdef,{0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80}},zero={0};WCHAR out[42];int i;
 logh=CreateFileA("C:\\FP32\\GUIDSTR.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA("C:\\FP32\\F32O.DLL");check(dll!=0,"DLL loaded ");
 format=dll?(void*)GetProcAddress(dll,"StringFromGUID2"):0;check(format!=0,"Export found ");if(!format)goto done;
 for(i=0;i<42;i++)out[i]=0xa55a;
 check(format(&g,out+1,39)==39,"Exact capacity returns 39 ");
 check(lstrcmpW(out+1,L"{01234567-89AB-CDEF-1020-304050607080}")==0,"Canonical text and byte order ");
 check(out[0]==0xa55a&&out[40]==0xa55a&&out[41]==0xa55a,"Guard characters preserved ");
 for(i=0;i<42;i++)out[i]=0xa55a;
 check(format(&g,out+1,38)==0,"Short capacity fails ");
 for(i=0;i<42&&out[i]==0xa55a;i++){}check(i==42,"Short output untouched ");
 check(format(&zero,out,42)==39&&lstrcmpW(out,L"{00000000-0000-0000-0000-000000000000}")==0,"Zero GUID and larger buffer ");
 check(format(&g,out,-1)==0&&format(&g,out,0)==0,"Invalid capacities rejected ");
done:value("Checks ",checks);value("Failures ",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
