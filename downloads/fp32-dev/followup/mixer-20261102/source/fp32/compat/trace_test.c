/* Native regression for opt-in diagnostics and always-visible missing APIs. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD size(void){HANDLE h=CreateFileA("C:\\FP32\\TRACE.LOG",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);DWORD n;if(h==INVALID_HANDLE_VALUE)return 0;n=GetFileSize(h,0);CloseHandle(h);return n;}
static BOOL child(LPCSTR arg,DWORD expected){STARTUPINFOA si;PROCESS_INFORMATION pi;char command[MAX_PATH+10];DWORD i,code;GetModuleFileNameA(0,command,MAX_PATH);lstrcatA(command," ");lstrcatA(command,arg);for(i=0;i<sizeof(si);i++)((BYTE*)&si)[i]=0;for(i=0;i<sizeof(pi);i++)((BYTE*)&pi)[i]=0;si.cb=sizeof(si);if(!CreateProcessA(0,command,0,0,FALSE,0,0,0,&si,&pi))return FALSE;CloseHandle(pi.hThread);if(WaitForSingleObject(pi.hProcess,30000)!=WAIT_OBJECT_0){CloseHandle(pi.hProcess);return FALSE;}GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);return code==expected;}
static LONG WINAPI test_filter(EXCEPTION_POINTERS*p){return p->ExceptionRecord->ExceptionCode==0xe0424242?EXCEPTION_CONTINUE_EXECUTION:EXCEPTION_CONTINUE_SEARCH;}
void WINAPI entry(void){char*c=GetCommandLineA(),mode=0,text[256];HMODULE m;HANDLE h;DWORD before,after,n,errors=0;typedef HANDLE(WINAPI *Open)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);typedef BOOL(WINAPI *Missing)(HANDLE,LPVOID,DWORD);Open open;Missing missing;while(*c){if(c[0]==' '&&c[1]=='/'&&c[2])mode=c[2];c++;}
 if(mode=='f'){
 typedef LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI*SetFilter)(LPTOP_LEVEL_EXCEPTION_FILTER);
 SetFilter install;LPTOP_LEVEL_EXCEPTION_FILTER diagnostic;EXCEPTION_RECORD record;CONTEXT context;EXCEPTION_POINTERS pointers;DWORD i;
 SetEnvironmentVariableA("FP32_TRACE",0);m=LoadLibraryA("C:\\FP32\\F32K.DLL");install=m?(SetFilter)GetProcAddress(m,"SetUnhandledExceptionFilter"):0;if(!install)ExitProcess(10);
 for(i=0;i<sizeof(record);i++)((BYTE*)&record)[i]=0;for(i=0;i<sizeof(context);i++)((BYTE*)&context)[i]=0;
 record.ExceptionCode=0xe0424242;record.ExceptionAddress=(void*)0x12345678;context.Eip=0x12345678;context.Esp=0x23456780;pointers.ExceptionRecord=&record;pointers.ContextRecord=&context;
 install(test_filter);diagnostic=SetUnhandledExceptionFilter(0);if(!diagnostic)ExitProcess(11);
 SetLastError(12345);if(diagnostic(&pointers)!=EXCEPTION_CONTINUE_EXECUTION||GetLastError()!=12345)ExitProcess(12);ExitProcess(0);
 }
 if(mode){SetEnvironmentVariableA("FP32_TRACE",mode=='t'?"1":0);if(mode=='m'){m=LoadLibraryA("D:\\F32P.DLL");missing=m?(Missing)GetProcAddress(m,"GetProcessMemoryInfo"):0;if(!missing)ExitProcess(9);missing(GetCurrentProcess(),0,0);ExitProcess(8);}m=LoadLibraryA("D:\\F32K.DLL");open=m?(Open)GetProcAddress(m,"CreateFileW"):0;if(!open)ExitProcess(7);h=open(L"C:\\FP32\\SAVETEST.SWF",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)ExitProcess(6);CloseHandle(h);ExitProcess(0);}
 before=size();if(!child("/q",0))errors++;after=size();if(after!=before)errors++;
 before=after;if(!child("/t",0))errors++;after=size();if(after<=before)errors++;
 before=after;if(!child("/m",120))errors++;after=size();if(after<=before)errors++;
 before=after;if(!child("/f",0))errors++;after=size();if(after<=before)errors++;
 n=wsprintfA(text,"Optional diagnostic trace test\r\nChecks: 8\r\nErrors: %lu\r\n",errors);h=CreateFileA("C:\\FP32\\TRACETST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(h!=INVALID_HANDLE_VALUE){WriteFile(h,text,n,&after,0);CloseHandle(h);}ExitProcess(errors?1:0);}
