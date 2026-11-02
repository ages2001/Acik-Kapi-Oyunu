/* Cross-player BSTR ownership and optional thread-ID integration probe. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static DWORD checks,failures,worker_errors;
static WCHAR large_source[524289];
typedef WCHAR* (WINAPI *Allocate)(LPCWSTR);
typedef void (WINAPI *Release)(WCHAR*);
typedef HANDLE(WINAPI *StartThread)(LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,PVOID,DWORD,LPDWORD);
typedef LONG(WINAPI *QueryProcess)(HANDLE,ULONG,PVOID,ULONG,PULONG);
static Allocate allocate;static Release release;static QueryProcess query;
static void number(const char*s,DWORD value){char b[200];DWORD n;wsprintfA(b,"%s %lu\r\n",s,value);WriteFile(logh,b,lstrlenA(b),&n,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
static BOOL committed(DWORD*out){DWORD vm[11],size=0;LONG status=query(GetCurrentProcess(),3,vm,sizeof(vm),&size);if(status<0||size!=sizeof(vm))return FALSE;*out=vm[9];return TRUE;}
static DWORD WINAPI worker(void *unused){DWORD i;WCHAR*s;(void)unused;for(i=0;i<1000;i++){s=allocate(L"Cross-DLL \x20ac \x011f");if(!s||((DWORD*)s)[-1]!=26||s[10]!=0x20ac||s[12]!=0x011f||s[13]!=0)InterlockedIncrement((LONG*)&worker_errors);release(s);}return 0x42;}
void WINAPI entry(void){HMODULE a,b,k;StartThread start;WCHAR*s;HANDLE threads[4];DWORD i,id,exitcode,before=0,during=0,after=0;
 logh=CreateFileA("C:\\FP32\\BSTR10.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 query=(QueryProcess)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtQueryInformationProcess");
 check(query!=0,"Native process counter provider");if(!query)goto done;
 a=LoadLibraryA("C:\\FP32\\D32A.DLL");b=LoadLibraryA("C:\\FP32\\D10A.DLL");k=LoadLibraryA("C:\\FP32\\D10K.DLL");check(a&&b&&k,"Production adapters load");
 allocate=a?(Allocate)GetProcAddress(a,"SysAllocString"):0;release=b?(Release)GetProcAddress(b,(LPCSTR)6):0;start=k?(StartThread)GetProcAddress(k,"CreateThread"):0;
 check(allocate&&release&&start,"Producer name, consumer ordinal and thread export resolve");if(!allocate||!release||!start)goto done;
 release(0);check(TRUE,"NULL release returns");check(!allocate(0),"NULL source remains NULL");
 s=allocate(L"");check(s&&((DWORD*)s)[-1]==0&&s[0]==0,"Empty BSTR layout");release(s);
 for(i=0;i<524288;i++)large_source[i]='Q';large_source[524288]=0;
 check(committed(&before),"Commit before large allocation");
 s=allocate(large_source);check(s&&((DWORD*)s)[-1]==1048576&&s[524287]=='Q'&&!s[524288],"One MiB BSTR contents and length");
 check(committed(&during)&&during>=before+1048576,"Large allocation increases native commit");
 release(s);
 check(committed(&after)&&after+1048576<=during,"Cross-player release returns committed memory");
 number("Commit before",before);number("Commit during",during);number("Commit after",after);
 for(i=0;i<4;i++){
  id=0;threads[i]=start(0,0,worker,0,0,i&1?&id:0);
  check(threads[i]!=0,i&1?"Worker with thread-ID output":"Worker with NULL thread-ID output");
  if(i&1)check(id!=0,"Thread ID returned");
 }
 for(i=0;i<4;i++)if(threads[i]){check(WaitForSingleObject(threads[i],30000)==WAIT_OBJECT_0,"Worker completed");check(GetExitCodeThread(threads[i],&exitcode)&&exitcode==0x42,"Worker exit code preserved");CloseHandle(threads[i]);}
 check(!worker_errors,"4000 Unicode cross-player allocation/free cycles");
done:
 number("Checks",checks);number("Failures",failures);number("Worker errors",worker_errors);CloseHandle(logh);ExitProcess(failures?2:0);
}
