/* Observe native legacy structures before constructing MEMORYSTATUSEX. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;
static void value(const char*s,DWORD n){char b[160];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);}
typedef LONG(WINAPI *QuerySystem)(ULONG,PVOID,ULONG,PULONG);
typedef LONG(WINAPI *QueryProcess)(HANDLE,ULONG,PVOID,ULONG,PULONG);
static QuerySystem system_query;static QueryProcess process_query;
static void snapshot(DWORD phase){DWORD basic[12],perf[79],quota[9],vm[12],n,i;LONG s;MEMORYSTATUS old;
 value("Phase",phase);
 for(i=0;i<12;i++)basic[i]=vm[i]=0xcccccccc;
 for(i=0;i<79;i++)perf[i]=0xcccccccc;
 for(i=0;i<9;i++)quota[i]=0xcccccccc;
 n=0;s=system_query(0,basic,44,&n);value("Basic status",s);value("Basic length",n);for(i=0;i<12;i++)value("Basic",basic[i]);
 n=0;s=system_query(2,perf,312,&n);value("Performance status",s);value("Performance length",n);for(i=0;i<16;i++)value("Performance",perf[i]);value("Performance guard",perf[78]);
 n=0;s=process_query(GetCurrentProcess(),1,quota,32,&n);value("Quota status",s);value("Quota length",n);for(i=0;i<9;i++)value("Quota",quota[i]);
 n=0;s=process_query(GetCurrentProcess(),3,vm,44,&n);value("VM status",s);value("VM length",n);value("Virtual size",vm[1]);value("Private commit",vm[9]);
 old.dwLength=sizeof(old);GlobalMemoryStatus(&old);value("Old physical total",old.dwTotalPhys);value("Old physical available",old.dwAvailPhys);value("Old pagefile total",old.dwTotalPageFile);value("Old pagefile available",old.dwAvailPageFile);value("Old virtual total",old.dwTotalVirtual);value("Old virtual available",old.dwAvailVirtual);
}
void WINAPI entry(void){HMODULE nt;void*p;
 logh=CreateFileA("C:\\FP32\\SYSMNAT.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 nt=GetModuleHandleA("NTDLL.DLL");system_query=(QuerySystem)GetProcAddress(nt,"NtQuerySystemInformation");process_query=(QueryProcess)GetProcAddress(nt,"NtQueryInformationProcess");
 if(!system_query||!process_query){CloseHandle(logh);ExitProcess(2);}
 snapshot(0);p=VirtualAlloc(0,16*1024*1024,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);value("Allocation",(DWORD)p);if(p){snapshot(1);VirtualFree(p,0,MEM_RELEASE);snapshot(2);}CloseHandle(logh);ExitProcess(p?0:3);
}
