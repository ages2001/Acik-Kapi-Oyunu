#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static DWORD checks,failures;static volatile LONG apc_count;static DWORD apc_thread;
static void value(const char*s,DWORD n){char b[180];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;value(s,ok);}
static void WINAPI callback(void*context,ULONG low,LONG high){if(context==(void*)0x12345678){apc_thread=GetCurrentThreadId();InterlockedIncrement(&apc_count);}}
void WINAPI entry(void){HMODULE dll;HANDLE timer=0;ULONG minimum,maximum,current,actual;DWORD version=GetVersion()&0xffff,tick;LARGE_INTEGER due;BOOLEAN previous=0;LONG status;
 LONG(WINAPI*query)(ULONG*,ULONG*,ULONG*);LONG(WINAPI*set_resolution)(ULONG,BOOLEAN,ULONG*);LONG(WINAPI*create)(HANDLE*,ULONG,void*);LONG(WINAPI*set)(HANDLE,LARGE_INTEGER*,void*,void*,BOOLEAN,BOOLEAN*);LONG(WINAPI*cancel)(HANDLE,BOOLEAN*);
 logh=CreateFileA("C:\\FP32\\MMTIMER.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 dll=LoadLibraryA("C:\\FP32\\FPNTM.DLL");check(dll!=0,"Native multimedia import bridge loads");if(!dll)goto done;
 query=(void*)GetProcAddress(dll,"NtQueryTimerResolution");set_resolution=(void*)GetProcAddress(dll,"NtSetTimerResolution");create=(void*)GetProcAddress(dll,"NtCreateTimer");set=(void*)GetProcAddress(dll,"NtSetTimer");cancel=(void*)GetProcAddress(dll,"NtCancelTimer");check(query&&set_resolution&&create&&set&&cancel,"Timer imports resolve");if(!query||!set_resolution||!create||!set||!cancel)goto done;
 check(query(&minimum,&maximum,&current)>=0&&minimum>=maximum&&current>=maximum&&current<=minimum,"Resolution bounds are consistent");value("Minimum resolution 100ns",minimum);value("Maximum resolution 100ns",maximum);value("Current resolution 100ns",current);
 if(version==0x0a03)check(minimum==maximum&&current==minimum,"NT3.1 advertises its fixed clock without finer timing");
 actual=0;status=set_resolution(10000,TRUE,&actual);value("Resolution request status",(DWORD)status);value("Actual requested resolution",actual);check(status>=0&&actual>0,"Resolution request succeeds");
 check(query(&minimum,&maximum,&current)>=0&&current==actual,"Requested resolution matches the real current clock");
 /* The NT3.51 PC HAL rounds a 10000 request to 9766 (PIT divisor).
    Do not replace its real answer with the advertised nominal minimum. */
 check(set_resolution(0,FALSE,&actual)>=0,"Resolution request released");
 check(set_resolution(0,FALSE,&actual)==(LONG)0xc0000245,"Unmatched release is rejected");
 status=create(&timer,0x1f0003,0);check(status>=0&&timer,"Original three-argument timer creation adapts");if(status<0||!timer)goto done;
 due.QuadPart=-1000000;check(set(timer,&due,0,0,0,&previous)>=0,"One-shot timer arms with correct native ABI");check(WaitForSingleObject(timer,2000)==WAIT_OBJECT_0,"Native timer really expires");
 due.QuadPart=-10000000;check(set(timer,&due,0,0,0,&previous)>=0&&cancel(timer,&previous)>=0,"Armed timer cancels");check(WaitForSingleObject(timer,0)==WAIT_TIMEOUT,"Canceled timer stays nonsignaled");
 due.QuadPart=-1000000;check(set(timer,&due,(void*)callback,(void*)0x12345678,0,&previous)>=0,"Original timer APC arms");
 tick=GetTickCount();while(!apc_count&&GetTickCount()-tick<2000)SleepEx(50,TRUE);
 check(apc_count==1&&apc_thread==GetCurrentThreadId(),"APC receives context on the requesting thread");
 tick=GetTickCount();while(GetTickCount()-tick<250)SleepEx(50,TRUE);
 check(apc_count==1,"One-shot APC does not repeat");
 due.QuadPart=-1000000;
 if((version&255)<4)check(set(timer,&due,0,0,TRUE,&previous)==(LONG)0xc0000002,"Unsupported resume request matches early NT behavior");
 check(cancel(timer,&previous)>=0,"Timer remains valid after rejected resume request");
 done:if(timer)CloseHandle(timer);if(dll)FreeLibrary(dll);value("Checks",checks);value("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);}
