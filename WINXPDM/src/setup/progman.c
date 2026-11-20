/* Program Manager command transport. SPDX-License-Identifier: GPL-3.0-or-later */
#include "platform.h"
#include "files.h"

#ifndef _WIN32
#include <dde.h>
static HWND server;
static int waiting, acknowledged;
static HGLOBAL pending;

LRESULT FARCB conversation(HWND window, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DDE_ACK) {
        if (!waiting) {
            if (!server) server = (HWND)wp;
            GlobalDeleteAtom(LOWORD(lp)); GlobalDeleteAtom(HIWORD(lp));
        } else if ((HWND)wp == server && (HGLOBAL)HIWORD(lp) == pending) {
            acknowledged = (LOWORD(lp) & 0x8000) ? 1 : -1;
        }
        return 0;
    }
    if (msg == WM_DDE_TERMINATE) {
        if ((HWND)wp == server) server = NULL;
        return 0;
    }
    return DefWindowProc(window,msg,wp,lp);
}

static int execute(const char *command)
{
    WNDCLASS wc = {0};
    HWND client;
    ATOM topic;
    DWORD start;
    MSG msg;
    LPSTR buf;
    int posted = 0;
    wc.hInstance = wxp_instance; wc.lpfnWndProc = conversation;
    wc.lpszClassName = "WXPDDE";
    RegisterClass(&wc);
    client=CreateWindow("WXPDDE","",0,0,0,0,0,NULL,NULL,wxp_instance,NULL);
    if (!client) return 0;
    server=NULL; waiting=0; acknowledged=0;
    topic=GlobalAddAtom("PROGMAN");
    SendMessage((HWND)0xffff,WM_DDE_INITIATE,(WPARAM)client,MAKELPARAM(topic,topic));
    GlobalDeleteAtom(topic);
    if (server) {
        pending=GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE,lstrlen(command)+1);
        if (pending && (buf=GlobalLock(pending)) != NULL) {
            lstrcpy(buf,command); GlobalUnlock(pending); waiting=1;
            posted=PostMessage(server,WM_DDE_EXECUTE,(WPARAM)client,MAKELPARAM(0,pending));
            start=GetTickCount();
            while (posted && !acknowledged && GetTickCount()-start < 5000UL) {
                if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
                    TranslateMessage(&msg); DispatchMessage(&msg);
                } else Yield();
            }
        }
        if (pending && (!posted || acknowledged)) GlobalFree(pending);
        if (server) PostMessage(server,WM_DDE_TERMINATE,(WPARAM)client,0);
    }
    DestroyWindow(client);
    return acknowledged==1;
}
#else
#include <ddeml.h>
static HDDEDATA CALLBACK dde_cb(UINT type,UINT fmt,HCONV conv,HSZ a,HSZ b,
                                HDDEDATA data,ULONG_PTR x,ULONG_PTR y)
{
    (void)type;(void)fmt;(void)conv;(void)a;(void)b;(void)data;(void)x;(void)y;
    return NULL;
}
static int execute(const char *command)
{
    DWORD id=0;
    HSZ topic;
    HCONV conn;
    int ok=0;
    if (DdeInitialize(&id,dde_cb,APPCMD_CLIENTONLY,0)!=DMLERR_NO_ERROR) return 0;
    topic=DdeCreateStringHandle(id,"PROGMAN",CP_WINANSI);
    conn=DdeConnect(id,topic,topic,NULL);
    if (conn) {
        ok=DdeClientTransaction((LPBYTE)command,lstrlen(command)+1,conn,0,0,
                                XTYP_EXECUTE,5000,NULL)!=NULL;
        DdeDisconnect(conn);
    }
    DdeFreeStringHandle(id,topic);
    DdeUninitialize(id);
    return ok;
}
#endif

int change_shortcuts(HWND parent,int create)
{
    char exe[PATH_CAP];
    static char command[4*PATH_CAP+160];
    (void)parent;
    if (create) {
        if (!path_join(exe,wxp_folder,"INFO.EXE")) return 0;
        /* Program Manager keeps the active group within the conversation. */
        wsprintf(command,"[CreateGroup(WINXPDM)][ShowGroup(WINXPDM,1)]"
                 "[AddItem(%s,WINXPDM Info)]"
                 "[AddItem(%s /remove,Uninstall WINXPDM,%s,1)]",exe,exe,exe);
        if (execute(command)) return 1;
        execute("[DeleteGroup(WINXPDM)]");
        return fail("Program Manager could not create the product icons.");
    }
    if (!execute("[DeleteGroup(WINXPDM)]")) return fail("Program Manager could not remove the product group.");
    return 1;
}
