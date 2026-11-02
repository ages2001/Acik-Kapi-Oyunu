/* Observe native VM_COUNTERS units before implementing the PSAPI adapter. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;
static void value(const char *s,DWORD n){char b[160],d[12];DWORD i=0,j=0,w;while(*s)b[i++]=*s++;do{d[j++]='0'+n%10;n/=10;}while(n);while(j)b[i++]=d[--j];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&w,0);}
typedef LONG(WINAPI *Query)(HANDLE,ULONG,PVOID,ULONG,PULONG);
static const char *labels[]={"PeakVirtualSize ","VirtualSize ","PageFaultCount ","PeakWorkingSetSize ","WorkingSetSize ","QuotaPeakPagedPoolUsage ","QuotaPagedPoolUsage ","QuotaPeakNonPagedPoolUsage ","QuotaNonPagedPoolUsage ","PagefileUsage ","PeakPagefileUsage "};
static void snapshot(Query q,DWORD phase){DWORD a[16],bytes=0,i;LONG status;for(i=0;i<16;i++)a[i]=0xccccccccu;status=q(GetCurrentProcess(),3,a,44,&bytes);value("Phase ",phase);value("NTSTATUS ",status);value("Returned bytes ",bytes);for(i=0;i<11;i++)value(labels[i],a[i]);value("Canary ",a[11]);}
void WINAPI entry(void){Query q;BYTE *p;DWORD i;SYSTEM_INFO sys;
 logh=CreateFileA("C:\\FP32\\MEMNAT.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 q=(Query)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtQueryInformationProcess");
 if(!q){value("Missing native query ",GetLastError());CloseHandle(logh);ExitProcess(2);}
 GetSystemInfo(&sys);value("Page size ",sys.dwPageSize);snapshot(q,0);
 p=VirtualAlloc(0,16*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);value("Allocation ",(DWORD)p);
 if(p){for(i=0;i<16*1024*1024;i+=sys.dwPageSize)p[i]=1;snapshot(q,1);VirtualFree(p,0,MEM_RELEASE);snapshot(q,2);}
 CloseHandle(logh);ExitProcess(p?0:3);
}
