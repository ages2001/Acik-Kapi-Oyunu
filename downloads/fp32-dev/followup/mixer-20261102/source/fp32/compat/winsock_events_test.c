#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
static HANDLE logh;static DWORD checks,failures;
static void number(const char*s,DWORD n){char b[180];DWORD w;wsprintfA(b,"%s %lu\r\n",s,n);WriteFile(logh,b,lstrlenA(b),&w,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;number(s,ok);}
void WINAPI entry(void)
{
    HMODULE dll;WSADATA data;HANDLE events[2]={0,0},duplicate=0;
    WSAEVENT(WINAPI*create)(void);BOOL(WINAPI*close_event)(WSAEVENT);DWORD error;
    logh=CreateFileA("C:\\FP32\\WSAEVENT.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
    check(WSAStartup(0x101,&data)==0,"Native Winsock initializes");
    dll=LoadLibraryA("C:\\FP32\\E32W.DLL");check(dll!=0,"Production proxy loads");
    create=dll?(void*)GetProcAddress(dll,"WSACreateEvent"):0;
    close_event=dll?(void*)GetProcAddress(dll,"WSACloseEvent"):0;
    check(create&&close_event,"Event exports resolve");if(!create||!close_event)goto done;
    events[0]=create();events[1]=create();check(events[0]&&events[1],"Two independent events created");
    if(!events[0]||!events[1])goto done;
    check(WaitForSingleObject(events[0],0)==WAIT_TIMEOUT,"Initial state is nonsignaled");
    check(SetEvent(events[0]),"Native event signaling succeeds");
    check(WaitForSingleObject(events[0],0)==WAIT_OBJECT_0,"Signaled event is waitable");
    check(WaitForSingleObject(events[0],0)==WAIT_OBJECT_0,"Manual-reset signal persists after wait");
    check(WaitForSingleObject(events[1],0)==WAIT_TIMEOUT,"Other event remains nonsignaled");
    check(WaitForMultipleObjects(2,events,TRUE,0)==WAIT_TIMEOUT,"Wait-all requires both signals");
    check(SetEvent(events[1])&&WaitForMultipleObjects(2,events,TRUE,0)==WAIT_OBJECT_0,"Wait-all succeeds when both signal");
    check(ResetEvent(events[0])&&WaitForSingleObject(events[0],0)==WAIT_TIMEOUT,"Explicit reset clears signal");
    check(DuplicateHandle(GetCurrentProcess(),events[1],GetCurrentProcess(),&duplicate,0,FALSE,DUPLICATE_SAME_ACCESS),"Native handle duplication succeeds");
    check(close_event(events[1]),"Winsock close releases original handle");events[1]=0;
    check(duplicate&&WaitForSingleObject(duplicate,0)==WAIT_OBJECT_0,"Duplicate retains shared event state");
    check(close_event(duplicate),"Winsock close accepts duplicated event");duplicate=0;
    WSASetLastError(0);check(!close_event(0),"NULL event is rejected");error=WSAGetLastError();
    check(error==WSA_INVALID_HANDLE,"Failure uses native Winsock last-error slot");
    check(close_event(events[0]),"Remaining event closes");events[0]=0;
done:
    if(events[0])CloseHandle(events[0]);if(events[1])CloseHandle(events[1]);if(duplicate)CloseHandle(duplicate);
    if(dll)FreeLibrary(dll);WSACleanup();number("Checks",checks);number("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
