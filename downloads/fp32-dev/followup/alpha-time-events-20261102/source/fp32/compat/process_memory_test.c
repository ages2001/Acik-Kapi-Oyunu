#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
static HANDLE logh;static DWORD checks,failures;
static void value(const char*s,DWORD n){char b[160],d[12];DWORD i=0,j=0,w;while(*s)b[i++]=*s++;do{d[j++]='0'+n%10;n/=10;}while(n);while(j)b[i++]=d[--j];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
void WINAPI entry(void){HMODULE dll;BOOL(WINAPI *query)(HANDLE,PPROCESS_MEMORY_COUNTERS,DWORD);DWORD box[14],i,e;PROCESS_MEMORY_COUNTERS_EX before,during,after;BYTE*p=0;HANDLE handle;BOOL ok;
 logh=CreateFileA("C:\\FP32\\MEMAPI.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA("C:\\FP32\\F32P.DLL");check(dll!=0,"DLL loaded ");query=dll?(void*)GetProcAddress(dll,"GetProcessMemoryInfo"):0;check(query!=0,"Export found ");if(!query)goto done;
 for(i=0;i<14;i++)box[i]=0xa5a5a5a5;
 ok=query(GetCurrentProcess(),(void*)(box+1),sizeof(PROCESS_MEMORY_COUNTERS));check(ok,"Basic query ");check(box[1]==40,"Basic output size ");check(box[0]==0xa5a5a5a5&&box[11]==0xa5a5a5a5,"Basic buffer bounds ");
 check(query(GetCurrentProcess(),(void*)&before,sizeof(before)),"Extended query ");
 check(before.cb==44&&before.WorkingSetSize>0&&before.PeakWorkingSetSize>=before.WorkingSetSize,"Extended size and working set ");
 check(before.PrivateUsage==before.PagefileUsage&&before.PeakPagefileUsage>=before.PagefileUsage,"Commit counters ");
 for(i=0;i<14;i++)box[i]=0xa5a5a5a5;
 SetLastError(0);ok=query(GetCurrentProcess(),(void*)(box+1),39);e=GetLastError();check(!ok&&e==ERROR_INSUFFICIENT_BUFFER,"Short buffer error ");
 for(i=0;i<14&&box[i]==0xa5a5a5a5;i++){}check(i==14,"Short buffer untouched ");
 SetLastError(0);ok=query((HANDLE)0x12345678,(void*)(box+1),40);e=GetLastError();check(!ok&&e==ERROR_INVALID_HANDLE,"Invalid handle translated ");
 for(i=0;i<14&&box[i]==0xa5a5a5a5;i++){}check(i==14,"Failed query output untouched ");
 handle=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,GetCurrentProcessId());ok=handle&&query(handle,(void*)&after,sizeof(after));if(handle)CloseHandle(handle);check(ok,"Real process handle ");
 handle=OpenProcess(SYNCHRONIZE,FALSE,GetCurrentProcessId());SetLastError(0);ok=handle&&query(handle,(void*)&after,sizeof(after));e=GetLastError();if(handle)CloseHandle(handle);check(handle&&!ok&&e==ERROR_ACCESS_DENIED,"Insufficient process rights ");
 p=VirtualAlloc(0,16*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);check(p!=0,"16 MiB allocation ");
 if(p){for(i=0;i<16*1024*1024;i+=4096)p[i]=1;check(query(GetCurrentProcess(),(void*)&during,sizeof(during)),"Query after commit ");
 value("Private bytes before ",before.PrivateUsage);value("Private bytes committed ",during.PrivateUsage);
 check(during.PrivateUsage>=before.PrivateUsage+16*1024*1024,"Committed bytes increased ");
 VirtualFree(p,0,MEM_RELEASE);p=0;check(query(GetCurrentProcess(),(void*)&after,sizeof(after)),"Query after release ");value("Private bytes released ",after.PrivateUsage);
 check(after.PrivateUsage+16*1024*1024<=during.PrivateUsage+65536,"Released bytes decreased ");}
done:if(p)VirtualFree(p,0,MEM_RELEASE);value("Checks ",checks);value("Failures ",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
