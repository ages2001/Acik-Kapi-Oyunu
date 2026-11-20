/* Run A:\\PMCHECK.EXE /cycle in a disposable NT Program Manager guest.
 * The test selects each target through Setup, checks the installed image and
 * executes the PCI diagnostic. The last target remains installed for reboot. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "wdm_target.h"

typedef struct { const char *label, *file; } TARGET;
#define WK_ROW(id, value, label, stem) { label, stem },
static const TARGET targets[] = { WK_WDM_TARGETS(WK_ROW) };
static char kit[MAX_PATH];
static FILE *logFile;

static HWND waitForButton(const char *title, const char *caption)
{
    DWORD start=GetTickCount();
    HWND window, button;
    char text[32];
    do {
        window=FindWindowA(NULL,title);
        button=window ? GetDlgItem(window,111) : NULL;
        if (button && GetWindowTextA(button,text,sizeof(text)) && !lstrcmpA(text,caption))
            return window;
        Sleep(100);
    } while (GetTickCount()-start<30000UL);
    return NULL;
}

static int runDiagnostic(const char *version, const char *reportName)
{
    char command[96];
    STARTUPINFOA startup;
    PROCESS_INFORMATION child;
    DWORD code=1;
    memset(&startup,0,sizeof(startup));
    startup.cb=sizeof(startup);
    sprintf(command,"C:\\WDMQA\\KITCHK.EXE /pci=%s",version);
    if (!CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) return 0;
    if (WaitForSingleObject(child.hProcess,30000UL)==WAIT_OBJECT_0)
        GetExitCodeProcess(child.hProcess,&code);
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    return !code && CopyFileA("C:\\WDMQA\\KITCHK.TXT",reportName,FALSE);
}

static int sameDriver(const char *stem)
{
    char source[40], target[MAX_PATH], a[1024], b[1024];
    FILE *in, *installed;
    size_t left, right;
    int equal=0;
    sprintf(source,"A:\\NT3\\%s.SYS",stem);
    sprintf(target,"%s\\WINWDM.SYS",kit);
    in=fopen(source,"rb");
    installed=fopen(target,"rb");
    if (in && installed) {
        do {
            left=fread(a,1,sizeof(a),in);
            right=fread(b,1,sizeof(b),installed);
            if (left!=right || memcmp(a,b,left)) break;
            if (left<sizeof(a)) equal=!ferror(in) && !ferror(installed);
        } while (left==sizeof(a));
    }
    if (in) fclose(in);
    if (installed) fclose(installed);
    return equal;
}

static int removeThroughWizard(void)
{
    char exe[MAX_PATH+4];
    HWND window;
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    memset(&startup,0,sizeof(startup));
    startup.cb=sizeof(startup);
    sprintf(exe,"%s\\UNINST.EXE",kit);
    if (!CreateProcessA(exe,NULL,NULL,NULL,FALSE,0,NULL,NULL,&startup,&process)) return 0;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    window=waitForButton("Remove WINWDM","&Remove");
    if (!window) return 0;
    PostMessageA(window,WM_COMMAND,111,0);
    window=waitForButton("Remove WINWDM","&Finish");
    if (!window) return 0;
    SendMessageA(window,WM_COMMAND,111,0);
    return 1;
}

static int installAndTest(int choice)
{
    HWND window,box;
    int n;
    char name[40],actual[16];
    if (WinExec("A:\\NT3\\SETUP.EXE",SW_SHOW)<=31) return 0;
    window=waitForButton("WINWDM Setup","&Next >");
    if (!window) return 0;
    SendMessageA(window,WM_COMMAND,111,0);
    window=waitForButton("WINWDM Setup","&Install");
    if (!window) return 0;
    box=GetDlgItem(window,114);
    if (!box || !IsWindowVisible(box) ||
        SendMessageA(box,CB_GETCOUNT,0,0)!=WK_TARGET_COUNT ||
        SendMessageA(box,CB_GETCURSEL,0,0)!=WK_INDEX_600) return 0;
    for (n=0;n<WK_TARGET_COUNT;++n) {
        if (SendMessageA(box,CB_GETLBTEXTLEN,n,0)>=sizeof(actual) ||
            SendMessageA(box,CB_GETLBTEXT,n,(LPARAM)actual)==CB_ERR ||
            lstrcmpA(actual,targets[n].label)) return 0;
    }
    if (SendMessageA(box,CB_SETCURSEL,choice,0)!=choice) return 0;
    fprintf(logFile,"WDM %s selected in Setup\n",targets[choice].label);
    fflush(logFile);
    PostMessageA(window,WM_COMMAND,111,0);
    window=waitForButton("WINWDM Setup","&Finish");
    if (!window || !sameDriver(targets[choice].file)) return 0;
    SendMessageA(window,WM_COMMAND,111,0);
    sprintf(name,"C:\\WDMQA\\SEL%d.TXT",choice);
    return runDiagnostic(targets[choice].label,name);
}

int PASCAL WinMain(HINSTANCE app,HINSTANCE previous,LPSTR args,int show)
{
    char marker[MAX_PATH];
    int i,ok=1;
    DWORD length=GetWindowsDirectoryA(kit,sizeof(kit));
    (void)app;(void)previous;(void)show;
    if (!length || length>sizeof(kit)-26) return 2;
    strcat(kit,"\\WINWDM");
    if (!lstrcmpiA(args,"/remove")) return removeThroughWizard() ? 0 : 1;
    if (lstrcmpiA(args,"/cycle")) return 2;
    sprintf(marker,"%s\\KIT.ID",kit);
    if (GetFileAttributesA(marker)!=(DWORD)-1) return 2;
    CreateDirectoryA("C:\\WDMQA",NULL);
    if (!CopyFileA("A:\\KITCHK.EXE","C:\\WDMQA\\KITCHK.EXE",FALSE)) return 2;
    logFile=fopen("C:\\WDMQA\\TARGETS.TXT","w");
    if (!logFile) return 2;
    for (i=0;ok && i<WK_TARGET_COUNT;++i) {
        ok=installAndTest(i);
        if (ok) {
            fputs("Installed kernel matches the selected image. Live PCI diagnostic passed.\n",logFile);
            if (i+1<WK_TARGET_COUNT) ok=removeThroughWizard();
        }
        fflush(logFile);
    }
    fprintf(logFile,"Target selection cycle %s\n",ok ? "PASS" : "FAIL");
    fclose(logFile);
    return ok ? 0 : 1;
}
