/* NT3.1 validation helper: inspect Program Manager DDE and set a test group's
 * current-user Startup preference. This is not a distribution utility. */
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <ddeml.h>
static void output(const char *s) { DWORD n; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, lstrlenA(s), &n, 0); }
static HDDEDATA CALLBACK callback(UINT t, UINT f, HCONV c, HSZ a, HSZ b, HDDEDATA d, ULONG_PTR x, ULONG_PTR y)
{ (void)t;(void)f;(void)c;(void)a;(void)b;(void)d;(void)x;(void)y;return 0; }
void Entry(void)
{
    char args[512], *p, *action, *group; DWORD instance=0, length; HSZ service, item;
    HCONV conversation; HDDEDATA data; HKEY key; LONG status; static char result[16384];
    if (lstrlenA(GetCommandLineA()) >= sizeof(args)) ExitProcess(2);
    lstrcpyA(args, GetCommandLineA()); p=args;
    if (*p=='"') {p++;while(*p&&*p!='"')p++;if(*p)p++;} else while(*p&&*p!=' ')p++;
    while(*p==' ')p++;action=p;while(*p&&*p!=' ')p++;if(*p)*p++=0;while(*p==' ')p++;group=p;
    if(!*group||lstrlenA(group)>120)ExitProcess(2);
    if(!lstrcmpA(action,"startup")) {
        status=RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows NT\\CurrentVersion\\Program Manager\\Settings",0,0,0,KEY_SET_VALUE,0,&key,0);
        if(status)ExitProcess(3);
        status=RegSetValueExA(key,"Startup",0,REG_SZ,(BYTE*)group,lstrlenA(group)+1);RegCloseKey(key);
        output(status?"FAIL Startup setting\r\n":"PASS Startup setting\r\n");ExitProcess(status?3:0);
    }
    if(!lstrcmpA(action,"sync")||!lstrcmpA(action,"twice")) {
        typedef BOOL (WINAPI *SYNC)(DWORD,LPSTR*,LPSTR*);
        HMODULE module=LoadLibraryA(group);SYNC sync;LPSTR message=0;BOOL ok;
        if(!module){output("FAIL loading setup extension\r\n");ExitProcess(7);}
        sync=(SYNC)GetProcAddress(module,"SyncStartup");
        if(!sync){output("FAIL resolving SyncStartup\r\n");FreeLibrary(module);ExitProcess(8);}
        ok=sync(0,0,&message);if(message){output(message);output("\r\n");}
        status=ok&&message&&!lstrcmpA(message,"Success")?0:9;
        if(!status&&!lstrcmpA(action,"twice")) {
            message=0;ok=sync(0,0,&message);
            if(message){output(message);output("\r\n");}
            status=ok&&message&&!lstrcmpA(message,"Success")?0:9;
        }
        FreeLibrary(module);ExitProcess(status);
    }
    if(lstrcmpA(action,"query")&&lstrcmpA(action,"execute"))ExitProcess(2);
    if(DdeInitializeA(&instance,callback,APPCMD_CLIENTONLY,0))ExitProcess(4);
    service=DdeCreateStringHandleA(instance,"PROGMAN",CP_WINANSI);
    conversation=DdeConnect(instance,service,service,0);if(!conversation)ExitProcess(5);
    if(!lstrcmpA(action,"execute")) {
        data=DdeClientTransaction((BYTE*)group,lstrlenA(group)+1,conversation,0,0,XTYP_EXECUTE,5000,0);
        output(data?"PASS DDE execute\r\n":"FAIL DDE execute\r\n");
        DdeDisconnect(conversation);DdeFreeStringHandle(instance,service);DdeUninitialize(instance);ExitProcess(data?0:6);
    }
    item=DdeCreateStringHandleA(instance,group,CP_WINANSI);
    data=DdeClientTransaction(0,0,conversation,item,CF_TEXT,XTYP_REQUEST,5000,0);
    if(data){length=DdeGetData(data,(BYTE*)result,sizeof(result)-1,0);result[length]=0;output(result);DdeFreeDataHandle(data);}
    else output("DDE request failed\r\n");
    DdeFreeStringHandle(instance,item);DdeDisconnect(conversation);DdeFreeStringHandle(instance,service);DdeUninitialize(instance);
    ExitProcess(data?0:6);
}
