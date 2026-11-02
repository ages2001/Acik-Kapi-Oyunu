#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static HANDLE logh;
static DWORD failures;
static void check(BOOL ok,const char *name){DWORD w;WriteFile(logh,ok?"PASS ":"FAIL ",5,&w,0);WriteFile(logh,name,lstrlenA(name),&w,0);WriteFile(logh,"\r\n",2,&w,0);FlushFileBuffers(logh);if(!ok)failures++;}
void WINAPI entry(void){
 HMODULE kernel=LoadLibraryA("C:\\FP32\\F32K.DLL"),shell=LoadLibraryA("SHELL32.DLL");
 FARPROC(WINAPI*resolve)(HMODULE,LPCSTR);BOOL(WINAPI*execute)(SHELLEXECUTEINFOW*);SHELLEXECUTEINFOW info;DWORD i,exitcode=0;
 logh=CreateFileA("C:\\FP32\\SHLTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 resolve=kernel?(void*)GetProcAddress(kernel,"GetProcAddress"):0;
 execute=resolve?(void*)resolve(shell,"ShellExecuteExW"):0;check(execute!=0,"dynamic ShellExecuteExW resolution");if(!execute)ExitProcess(2);
 check(resolve(shell,"ShellExecuteW")==GetProcAddress(shell,"ShellExecuteW"),"existing native export preserved");
 for(i=0;i<sizeof(info);i++)((BYTE*)&info)[i]=0;
 info.cbSize=sizeof(info);info.fMask=SEE_MASK_NOCLOSEPROCESS;info.lpFile=L"C:\\FP32\\SHCHILD.EXE";info.nShow=SW_HIDE;
 check(execute(&info),"launch real child executable");check(info.hProcess!=0,"return real process handle");
 if(info.hProcess){check(WaitForSingleObject(info.hProcess,5000)==WAIT_OBJECT_0,"wait for child completion");check(GetExitCodeProcess(info.hProcess,&exitcode)&&exitcode==67,"child exit code is 67");check(CloseHandle(info.hProcess),"process handle is closable");}
 check(!execute(0)&&GetLastError()==ERROR_INVALID_PARAMETER,"null structure rejected");
 info.fMask=SEE_MASK_IDLIST;check(!execute(&info)&&GetLastError()==ERROR_NOT_SUPPORTED,"unsupported ID-list mode rejected");
 CloseHandle(logh);ExitProcess(failures?1:0);
}
