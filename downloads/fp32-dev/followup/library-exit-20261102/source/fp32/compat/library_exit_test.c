#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef void(WINAPI *Exit)(HMODULE,DWORD);
typedef DWORD(WINAPI *Count)(void);
static Exit leave;static HANDLE logh;static DWORD checks,failures;
typedef struct {HMODULE module;DWORD code;}Job;
static void number(const char*s,DWORD n){char b[160];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
static DWORD WINAPI worker(void*p){Job*j=p;leave(j->module,j->code);return 0xbad;}
static void exercise(HMODULE module,DWORD code,Count count){Job job={module,code};DWORD before=count(),id,result;HANDLE t;
 t=CreateThread(0,0,worker,&job,0,&id);check(t!=0,"Worker starts");if(!t)return;
 check(WaitForSingleObject(t,10000)==WAIT_OBJECT_0,"Worker exits");
 check(GetExitCodeThread(t,&result)&&result==code,"Requested thread exit code preserved");
 check(count()==before+1,"Native thread detach notifications delivered");CloseHandle(t);}
void WINAPI entry(void){HMODULE proxy,observer,other;Count count;
 logh=CreateFileA("C:\\FP32\\LIBEXIT.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 observer=LoadLibraryA("C:\\FP32\\EXITOBS.DLL");proxy=LoadLibraryA("C:\\FP32\\X32K.DLL");
 check(observer&&proxy,"Production proxy and observer load");
 leave=proxy?(Exit)GetProcAddress(proxy,"FreeLibraryAndExitThread"):0;count=observer?(Count)GetProcAddress(observer,"GetDetachCount"):0;
 check(leave&&count,"Exit API and observer resolve");if(!leave||!count)goto done;
 other=LoadLibraryA("C:\\FP32\\EXITDROP.DLL");check(other!=0,"Separate unload target loads");
 if(other){exercise(other,0x42,count);check(!GetModuleHandleA("EXITDROP.DLL"),"Separate target unloaded");check(GetModuleHandleA("X32K.DLL")==proxy,"Proxy retained after other-module unload");}
 exercise(0,0x73,count);check(GetModuleHandleA("X32K.DLL")==proxy,"Failed unload still exits thread without unloading proxy");
 exercise(proxy,0x12345678,count);check(!GetModuleHandleA("X32K.DLL"),"Proxy itself unloaded before thread exit");proxy=0;
done:number("Checks",checks);number("Failures",failures);CloseHandle(logh);if(proxy)FreeLibrary(proxy);if(observer)FreeLibrary(observer);ExitProcess(failures?2:0);}
