#include "kit.h"
#include <string.h>

/* The Win16 implementation uses the original Program Manager DDE protocol
 * because DDEML did not ship with Windows 3.0. */
#ifdef _WIN32
#include <ddeml.h>
static DWORD instance;
static HCONV conversation;
static HDDEDATA CALLBACK callback(UINT type,UINT format,HCONV c,HSZ a,HSZ b,
    HDDEDATA data,ULONG_PTR x,ULONG_PTR y)
{ (void)type;(void)format;(void)c;(void)a;(void)b;(void)data;(void)x;(void)y; return 0; }

LRESULT WkDdeMessage(HWND w,UINT m,WPARAM p,LPARAM l) {return DefWindowProc(w,m,p,l);}
static BOOL wkConnect(HWND owner)
{
    HSZ atom;
    (void)owner;
    if (DdeInitialize(&instance,callback,APPCMD_CLIENTONLY,0)!=DMLERR_NO_ERROR) return FALSE;
    atom=DdeCreateStringHandle(instance,"PROGMAN",CP_WINANSI);
    conversation=DdeConnect(instance,atom,atom,NULL);
    DdeFreeStringHandle(instance,atom);
    if (!conversation) { DdeUninitialize(instance); instance=0; }
    return conversation!=0;
}
static BOOL execute(HWND owner,char *cmd)
{
    (void)owner;
    return DdeClientTransaction((LPBYTE)cmd,(DWORD)strlen(cmd)+1,conversation,0,0,
                                XTYP_EXECUTE,5000,NULL)!=0;
}
static void wkDisconnect(HWND owner)
{ (void)owner; DdeDisconnect(conversation); DdeUninitialize(instance); instance=0; }
#else
#include <dde.h>
static HWND server;
static BOOL inviting, answered, accepted;
static HGLOBAL pending;

LRESULT WkDdeMessage(HWND owner,UINT m,WPARAM w,LPARAM l)
{
    if (m==WM_DDE_ACK && inviting) {
        GlobalDeleteAtom(LOWORD(l)); GlobalDeleteAtom(HIWORD(l));
        if (!server) server=(HWND)w;
        else if (server!=(HWND)w) PostMessage((HWND)w,WM_DDE_TERMINATE,(WPARAM)owner,0);
    } else if (m==WM_DDE_ACK && server==(HWND)w && (HGLOBAL)HIWORD(l)==pending) {
        accepted=!!(LOWORD(l)&0x8000); answered=TRUE;
    } else if (m==WM_DDE_TERMINATE && (HWND)w==server) {
        server=NULL; PostMessage((HWND)w,WM_DDE_TERMINATE,(WPARAM)owner,0);
    }
    return 0;
}
static BOOL wkConnect(HWND owner)
{
    ATOM a=GlobalAddAtom("PROGMAN");
    server=NULL; inviting=TRUE;
    SendMessage(HWND_BROADCAST,WM_DDE_INITIATE,(WPARAM)owner,MAKELONG(a,a));
    inviting=FALSE; GlobalDeleteAtom(a);
    return server!=NULL;
}
static BOOL execute(HWND owner,char *cmd)
{
    MSG msg; DWORD start=GetTickCount();
    char far *p;
    pending=GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE,strlen(cmd)+1);
    if (!pending) return FALSE;
    p=GlobalLock(pending);
    if (!p) { GlobalFree(pending); return FALSE; }
    lstrcpy(p,cmd); GlobalUnlock(pending);
    answered=accepted=FALSE;
    if (!PostMessage(server,WM_DDE_EXECUTE,(WPARAM)owner,MAKELONG(0,pending))) {
        GlobalFree(pending); return FALSE;
    }
    while (!answered && server && GetTickCount()-start<5000UL) {
        if (PeekMessage(&msg,0,WM_DDE_FIRST,WM_DDE_LAST,PM_REMOVE)) DispatchMessage(&msg);
        else Yield();
    }
    if (answered) GlobalFree(pending);
    pending=NULL;
    return answered && accepted;
}
static void wkDisconnect(HWND owner)
{
    if (server) PostMessage(server,WM_DDE_TERMINATE,(WPARAM)owner,0);
    server=NULL;
}
#endif

BOOL WkGroup(HWND owner,const char *dir,BOOL install)
{
    char command[2*PATH_CAP+90]; BOOL ok;
    if (!wkConnect(owner)) return FALSE;
    if (install) {
        ok=execute(owner,"[CreateGroup(WINWDM)]");
        if (ok) {
            wsprintf(command,"[AddItem(%s\\INFO.EXE,WINWDM Info,%s\\INFO.EXE,0)]",dir,dir);
            ok=execute(owner,command);
        }
        if (ok) {
            wsprintf(command,"[AddItem(%s\\UNINST.EXE,Uninstall WINWDM,%s\\UNINST.EXE,0)]",dir,dir);
            ok=execute(owner,command);
        }
    } else ok=execute(owner,"[DeleteGroup(WINWDM)]");
    wkDisconnect(owner);
    return ok;
}
