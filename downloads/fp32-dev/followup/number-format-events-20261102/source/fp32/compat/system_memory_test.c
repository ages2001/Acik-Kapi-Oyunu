/* Native integration test against the linked production kernel proxy. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static DWORD checks,failures;
static void number(const char*s,DWORD v){char b[160];DWORD n;wsprintfA(b,"%s %lu\r\n",s,v);WriteFile(logh,b,lstrlenA(b),&n,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
static BOOL close_value(ULONGLONG a,ULONGLONG b,DWORD tolerance){return a>b?a-b<=tolerance:b-a<=tolerance;}
typedef BOOL(WINAPI *ReadMemory)(LPMEMORYSTATUSEX);
static BOOL sample(ReadMemory query,MEMORYSTATUSEX*s){s->dwLength=sizeof(*s);return query(s);}
static void invariants(const MEMORYSTATUSEX*s){
 check(s->dwMemoryLoad<=100,"Memory load is a percentage");
 check(s->ullTotalPhys&&s->ullAvailPhys<=s->ullTotalPhys,"Physical memory bounds");
 check(s->ullTotalPageFile&&s->ullAvailPageFile<=s->ullTotalPageFile,"Commit memory bounds");
 check(s->ullTotalVirtual&&s->ullAvailVirtual<=s->ullTotalVirtual,"Virtual address bounds");
 check(!s->ullAvailExtendedVirtual,"Reserved field is zero");
}
void WINAPI entry(void){HMODULE module;ReadMemory query;MEMORYSTATUSEX before,reserved,committed,after;
 struct{DWORD pre;MEMORYSTATUSEX data;DWORD post;}guard;
 MEMORYSTATUS old;SYSTEM_INFO info;void*p;DWORD i;BOOL ok;
 logh=CreateFileA("C:\\FP32\\SYSMEM.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 module=LoadLibraryA("C:\\FP32\\G32K.DLL");check(module!=0,"Production kernel adapter loads");
 query=module?(ReadMemory)GetProcAddress(module,"GlobalMemoryStatusEx"):0;check(query!=0,"GlobalMemoryStatusEx resolves");if(!query)goto done;
 SetLastError(0);check(!query(0)&&GetLastError()==ERROR_INVALID_PARAMETER,"NULL output rejected");
 for(i=0;i<sizeof(guard);i++)((BYTE*)&guard)[i]=0xa5;guard.data.dwLength=sizeof(guard.data)-1;
 SetLastError(0);ok=query(&guard.data);check(!ok&&GetLastError()==ERROR_INVALID_PARAMETER,"Wrong structure length rejected");
 ok=TRUE;for(i=4;i<sizeof(guard.data);i++)if(((BYTE*)&guard.data)[i]!=0xa5)ok=FALSE;
 check(ok&&guard.pre==0xa5a5a5a5&&guard.post==0xa5a5a5a5,"Failed call leaves output and guards unchanged");
 guard.data.dwLength=sizeof(guard.data);check(query(&guard.data),"Valid output structure accepted");
 check(guard.pre==0xa5a5a5a5&&guard.post==0xa5a5a5a5,"Successful call preserves output guards");
 before=guard.data;invariants(&before);GetSystemInfo(&info);old.dwLength=sizeof(old);GlobalMemoryStatus(&old);
 check(before.ullTotalPhys==old.dwTotalPhys,"Physical total matches native legacy query");
 check(close_value(before.ullAvailPhys,old.dwAvailPhys,1024*1024),"Available physical matches native query within sampling tolerance");
 check(before.ullTotalPageFile==old.dwTotalPageFile,"Commit total matches native legacy query");
 check(close_value(before.ullAvailPageFile,old.dwAvailPageFile,1024*1024),"Available commit matches native query within sampling tolerance");
 check(before.ullTotalVirtual==(ULONGLONG)(DWORD)info.lpMaximumApplicationAddress-(DWORD)info.lpMinimumApplicationAddress+1,"Virtual total matches native address range");
 check(before.ullAvailVirtual==old.dwAvailVirtual,"Free virtual matches native query");
 number("Physical total low",(DWORD)before.ullTotalPhys);number("Physical total high",(DWORD)(before.ullTotalPhys>>32));
 p=VirtualAlloc(0,16*1024*1024,MEM_RESERVE,PAGE_READWRITE);check(p!=0,"Reserve sixteen MiB");
 if(p){
  check(sample(query,&reserved),"Query after reservation");
  check(before.ullAvailVirtual==reserved.ullAvailVirtual+16*1024*1024,"Reservation removes exactly sixteen MiB of virtual space");
  check(close_value(before.ullAvailPageFile,reserved.ullAvailPageFile,1024*1024),"Reservation does not consume sixteen MiB of commit");
  check(VirtualAlloc(p,16*1024*1024,MEM_COMMIT,PAGE_READWRITE)==p,"Commit reserved memory");
  check(sample(query,&committed),"Query after commitment");
  check(close_value(reserved.ullAvailPageFile,committed.ullAvailPageFile+16*1024*1024,1024*1024),"Commitment consumes sixteen MiB within sampling tolerance");
  check(reserved.ullAvailVirtual==committed.ullAvailVirtual,"Commitment does not reserve more address space");
  check(VirtualFree(p,0,MEM_RELEASE),"Release reservation and commitment");
  check(sample(query,&after),"Query after release");
  check(after.ullAvailVirtual==before.ullAvailVirtual,"Released virtual space restored exactly");
  check(close_value(after.ullAvailPageFile,before.ullAvailPageFile,1024*1024),"Released commit restored within sampling tolerance");
  check(after.ullTotalPhys==before.ullTotalPhys&&after.ullTotalVirtual==before.ullTotalVirtual,"Totals remain stable");
 }
done:number("Checks",checks);number("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
