#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "timezone_fixtures.h"
void *memcpy(void*d,const void*s,unsigned n){BYTE*a=d;const BYTE*b=s;while(n--)*a++=*b++;return d;}
typedef BOOL(WINAPI*Convert)(const TIME_ZONE_INFORMATION*,const SYSTEMTIME*,SYSTEMTIME*);
static HANDLE logh;static DWORD checks,failures;
static void number(const char*s,DWORD n){char b[180];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
static void zero(void*p,UINT n){BYTE*b=p;while(n--)*b++=0;}
static BOOL equal(const SYSTEMTIME*a,const SYSTEMTIME*b){const WORD*x=(const WORD*)a,*y=(const WORD*)b;UINT i;for(i=0;i<8;i++)if(x[i]!=y[i])return FALSE;return TRUE;}
static void rule(SYSTEMTIME*r,WORD month,WORD week,WORD hour){zero(r,sizeof(*r));r->wMonth=month;r->wDay=week;r->wHour=hour;}
void WINAPI entry(void){HMODULE dll;Convert convert;TIME_ZONE_INFORMATION zones[5],z;SYSTEMTIME out,in,sentinel;UINT i,n,bad=0;DWORD last;
 logh=CreateFileA("C:\\FP32\\TZTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA("C:\\FP32\\T32K.DLL");check(dll!=0,"Production kernel proxy loads");convert=dll?(void*)GetProcAddress(dll,"SystemTimeToTzSpecificLocalTime"):0;check(convert!=0,"Conversion export resolves");if(!convert)goto done;
 zero(zones,sizeof(zones));zones[0].Bias=300;zones[1].Bias=-60;zones[2].Bias=-600;zones[3].Bias=-720;zones[4].Bias=-330;
 for(i=0;i<4;i++)zones[i].DaylightBias=-60;
 rule(&zones[0].DaylightDate,3,2,2);rule(&zones[0].StandardDate,11,1,2);
 rule(&zones[1].DaylightDate,3,5,2);rule(&zones[1].StandardDate,10,5,3);
 rule(&zones[2].DaylightDate,10,1,2);rule(&zones[2].StandardDate,4,1,3);
 rule(&zones[3].DaylightDate,9,5,2);rule(&zones[3].StandardDate,4,1,3);
 n=sizeof(vectors)/sizeof(vectors[0]);for(i=0;i<n;i++){
  if(!convert(&zones[vectors[i].zone],&vectors[i].utc,&out)||!equal(&out,&vectors[i].local)){if(bad<10){number("Failed vector index",i);number("Error",GetLastError());}bad++;}
 }
 number("Independent IANA vectors",n);number("Vector mismatches",bad);check(bad==0,"Daily and exact DST-boundary oracle results");
 in=vectors[0].utc;out=in;check(convert(&zones[0],&out,&out)&&equal(&out,&vectors[0].local),"Input and output may alias");
 /* Fixed offsets must ignore the unused seasonal biases. */
 z=zones[4];z.StandardBias=999;z.DaylightBias=-999;check(convert(&z,&vectors[n-1].utc,&out)&&equal(&out,&vectors[n-1].local),"No-DST timezone ignores seasonal biases");
 /* Explicit 2024 cutover dates match the recurring US rule at its boundary. */
 z=zones[0];z.DaylightDate.wYear=z.StandardDate.wYear=2024;z.DaylightDate.wDay=10;z.StandardDate.wDay=3;
 zero(&in,sizeof(in));in.wYear=2024;in.wMonth=3;in.wDay=10;in.wHour=7;
 check(convert(&z,&in,&out)&&out.wHour==3&&out.wDay==10,"Absolute transition date at spring boundary");
 in.wMonth=11;in.wDay=3;in.wHour=6;check(convert(&z,&in,&out)&&out.wHour==1&&out.wDay==3,"Absolute transition date at fall boundary");
 /* Nonzero standard bias contributes when both seasonal rules exist. */
 z=zones[0];z.Bias=240;z.StandardBias=60;in.wMonth=1;in.wDay=15;in.wHour=12;
 check(convert(&z,&in,&out)&&out.wHour==7,"Nonzero standard bias is applied");
 zero(&sentinel,sizeof(sentinel));sentinel.wYear=0xa5a5;out=sentinel;
 SetLastError(0);check(!convert(&zones[0],0,&out)&&GetLastError()==ERROR_INVALID_PARAMETER&&equal(&out,&sentinel),"NULL input rejected without output mutation");
 SetLastError(0);check(!convert(&zones[0],&in,0)&&GetLastError()==ERROR_INVALID_PARAMETER,"NULL output rejected");
 in.wMonth=2;in.wDay=30;out=sentinel;SetLastError(0);check(!convert(&zones[0],&in,&out)&&GetLastError()==ERROR_INVALID_PARAMETER&&equal(&out,&sentinel),"Invalid calendar date rejected");
 in.wMonth=3;in.wDay=10;z=zones[0];z.DaylightDate.wDay=6;out=sentinel;SetLastError(0);check(!convert(&z,&in,&out)&&GetLastError()==ERROR_INVALID_PARAMETER&&equal(&out,&sentinel),"Invalid recurring week rejected");
 z=zones[0];z.StandardDate.wMonth=0;check(!convert(&z,&in,&out),"Incomplete seasonal rule rejected");
 z=zones[0];z.DaylightDate.wDayOfWeek=7;check(!convert(&z,&in,&out),"Invalid recurring weekday rejected");
 zero(&z,sizeof(z));z.Bias=60;zero(&in,sizeof(in));in.wYear=1601;in.wMonth=in.wDay=1;out=sentinel;check(!convert(&z,&in,&out)&&equal(&out,&sentinel),"FILETIME epoch underflow rejected");
 GetSystemTime(&in);last=GetTimeZoneInformation(&z);check(last!=TIME_ZONE_ID_INVALID&&convert(0,&in,&out)&&convert(&z,&in,&sentinel)&&equal(&out,&sentinel),"NULL timezone uses native system configuration");
 done:number("Checks",checks);number("Failures",failures);if(dll)FreeLibrary(dll);CloseHandle(logh);ExitProcess(failures?2:0);
}
