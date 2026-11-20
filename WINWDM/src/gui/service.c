#include "kit.h"
#include <winsvc.h>

static void detachFilter(void)
{
    DWORD bytes;
    HANDLE dev=CreateFileA("\\\\.\\WdmKit",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,
                           NULL,OPEN_EXISTING,0,NULL);
    if (dev!=INVALID_HANDLE_VALUE) {
        DeviceIoControl(dev,WK_IOCTL_QUIESCE,NULL,0,NULL,0,&bytes,NULL);
        CloseHandle(dev);
    }
}

/* The named service belongs to this kit. Do not replace an existing service. */
BOOL WkService(const char *path, BOOL install)
{
    SC_HANDLE manager, service;
    SERVICE_STATUS state;
    DWORD start;
    BOOL ok=FALSE;
    if (install && GetFileAttributesA(path)==(DWORD)-1) return FALSE;
    manager=OpenSCManagerA(NULL,NULL,install ? SC_MANAGER_CREATE_SERVICE : SC_MANAGER_CONNECT);
    if (!manager) return FALSE;
    if (install) {
        service=CreateServiceA(manager,"WdmKit",KIT_TITLE,SERVICE_START|DELETE,
            SERVICE_KERNEL_DRIVER,SERVICE_SYSTEM_START,SERVICE_ERROR_NORMAL,
            "\\SystemRoot\\" KIT_DIR "\\WINWDM.SYS",NULL,NULL,NULL,NULL,NULL);
        if (service) {
            ok=StartServiceA(service,0,NULL);
            if (!ok) DeleteService(service);
        }
    } else {
        service=OpenServiceA(manager,"WdmKit",SERVICE_STOP|SERVICE_QUERY_STATUS|DELETE);
        if (!service && GetLastError()==ERROR_SERVICE_DOES_NOT_EXIST) ok=TRUE;
        if (service && QueryServiceStatus(service,&state)) {
            if (state.dwCurrentState!=SERVICE_STOPPED && state.dwCurrentState!=SERVICE_STOP_PENDING) {
                detachFilter();
                ControlService(service,SERVICE_CONTROL_STOP,&state);
            }
            start=GetTickCount();
            while (state.dwCurrentState!=SERVICE_STOPPED && GetTickCount()-start<5000UL) {
                Sleep(100);
                if (!QueryServiceStatus(service,&state)) break;
            }
            if (state.dwCurrentState==SERVICE_STOPPED) ok=DeleteService(service);
        }
    }
    if (service) CloseServiceHandle(service);
    CloseServiceHandle(manager);
    return ok;
}
