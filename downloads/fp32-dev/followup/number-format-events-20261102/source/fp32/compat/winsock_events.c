/* Winsock event objects use native manual-reset, initially nonsignaled events.
   This does not implement WSAEventSelect or network protocol support. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

WSAEVENT WINAPI fp_WSACreateEvent(void)
{
    HANDLE event=CreateEventA(0,TRUE,FALSE,0);
    if(!event){
        DWORD error=GetLastError();
        WSASetLastError(error==ERROR_OUTOFMEMORY?WSA_NOT_ENOUGH_MEMORY:error);
    }
    return event;
}

BOOL WINAPI fp_WSACloseEvent(WSAEVENT event)
{
    if(CloseHandle(event))return TRUE;
    WSASetLastError(GetLastError());
    return FALSE;
}
