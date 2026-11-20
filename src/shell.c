#include "common.h"
#include <ddeml.h>
static HDDEDATA CALLBACK callback(UINT type, UINT fmt, HCONV conv, HSZ a, HSZ b, HDDEDATA data,
                                  ULONG_PTR x, ULONG_PTR y) {
    (void)type;
    (void)fmt;
    (void)conv;
    (void)a;
    (void)b;
    (void)data;
    (void)x;
    (void)y;
    return 0;
}
static int command(HCONV c, const char *str) {
    return DdeClientTransaction((LPBYTE)str, (DWORD)strlen(str) + 1, c, 0, 0, XTYP_EXECUTE, 5000,
                                0) != 0;
}
int icons(int remove) {
    DWORD id = 0;
    HSZ svc;
    HCONV c;
    char line[MAX_PATH * 3 + 110];
    int ok = 0;
    if (DdeInitializeA(&id, callback, APPCMD_CLIENTONLY, 0) != DMLERR_NO_ERROR)
        return 0;
    svc = DdeCreateStringHandleA(id, "PROGMAN", CP_WINANSI);
    c = DdeConnect(id, svc, svc, 0);
    if (c) {
        if (remove) {
            if (command(c, "[ShowGroup(NT3SCPRT,1)]")) {
                command(c, "[DeleteItem(NT3SCPRT)]");
                command(c, "[DeleteItem(Uninstall NT3SCPRT)]");
                /* Keep a group if the user added items of their own. */
                HSZ group = DdeCreateStringHandleA(id, "NT3SCPRT", CP_WINANSI);
                HDDEDATA info =
                    DdeClientTransaction(0, 0, c, group, CF_TEXT, XTYP_REQUEST, 3000, 0);
                if (info) {
                    DWORD size;
                    BYTE *s = DdeAccessData(info, &size);
                    DWORD i;
                    if (s) {
                        for (i = 0; i < size && s[i] && s[i] != '\n'; i++)
                            ;
                        if (i < size && (i + 1 == size || !s[i + 1] || s[i + 1] == '\r'))
                            command(c, "[DeleteGroup(NT3SCPRT)]");
                        DdeUnaccessData(info);
                    }
                    DdeFreeDataHandle(info);
                }
                DdeFreeStringHandle(id, group);
            }
            ok = 1;
        } else if (command(c, "[CreateGroup(NT3SCPRT)]")) {
            command(c, "[ReplaceItem(NT3SCPRT)]");
            wsprintfA(line, "[AddItem(\"%s\",NT3SCPRT,\"%s\",0)]", appfile, appfile);
            ok = command(c, line);
            if (ok) {
                command(c, "[ReplaceItem(Uninstall NT3SCPRT)]");
                wsprintfA(line, "[AddItem(\"%s /remove\",Uninstall NT3SCPRT,\"%s\",1)]", appfile,
                          appfile);
                ok = command(c, line);
            }
        }
        DdeDisconnect(c);
    }
    DdeFreeStringHandle(id, svc);
    DdeUninitialize(id);
    return ok;
}
