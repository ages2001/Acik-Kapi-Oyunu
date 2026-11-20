/* /remove invokes the registered uninstaller. /open-remove also verifies removal
 * with Info open, and /verify checks the file and registration after a restart. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int PASCAL WinMain(HINSTANCE app, HINSTANCE previous, LPSTR args, int show)
{
    const char *entry="Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\WdmKit";
    char root[MAX_PATH], registration[MAX_PATH+3], expected[MAX_PATH+3],
         display[40], command[MAX_PATH+8], info[MAX_PATH], folder[MAX_PATH], output[MAX_PATH], *leaf;
    STARTUPINFOA startup;
    PROCESS_INFORMATION child;
    HKEY key;
    DWORD bytes, type, exitCode, start, length;
    BOOL removed=FALSE, verifyOnly=!lstrcmpiA(args,"/verify"), openInfo=!lstrcmpiA(args,"/open-remove");
    FILE *report;
    (void)previous; (void)show;
    if (!verifyOnly && !openInfo && lstrcmpiA(args,"/remove")) return 2;
    length=GetWindowsDirectoryA(root,sizeof(root));
    if (!length || length>=sizeof(root)-24) return 2;
    sprintf(expected,"\"%s\\WINWDM\\UNINST.EXE\"",root);
    sprintf(info,"%s\\WINWDM\\INFO.EXE",root);
    sprintf(folder,"%s\\WINWDM",root);
    length=GetModuleFileNameA(app,output,sizeof(output));
    if (!length || length>=sizeof(output)) return 2;
    leaf=strrchr(output,'\\');
    if (!leaf || leaf-output>MAX_PATH-16) return 2;
    strcpy(leaf+1,verifyOnly ? "REMBOOT.TXT" : "REMOVAL.TXT");
    report=fopen(output,"w");
    if (!report) return 2;
    if (verifyOnly) goto verify;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,entry,0,KEY_QUERY_VALUE,&key)) goto done;
    bytes=sizeof(display);
    if (RegQueryValueExA(key,"DisplayName",NULL,&type,(LPBYTE)display,&bytes) ||
        type!=REG_SZ || !bytes || bytes>sizeof(display) || display[bytes-1] || strcmp(display,"WINWDM")) {
        RegCloseKey(key);
        goto done;
    }
    bytes=sizeof(registration);
    if (RegQueryValueExA(key,"UninstallString",NULL,&type,(LPBYTE)registration,&bytes) ||
        type!=REG_SZ || !bytes || bytes>sizeof(registration) || registration[bytes-1] ||
        lstrcmpiA(registration,expected)) {
        RegCloseKey(key);
        goto done;
    }
    RegCloseKey(key);
    fprintf(report,"Registered command %s\n",registration);
    memset(&startup,0,sizeof(startup));
    startup.cb=sizeof(startup);
    if (openInfo) {
        BOOL found=FALSE;
        if (!CreateProcessA(info,NULL,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) goto done;
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        start=GetTickCount();
        do {
            found=FindWindowA("WINWDM Info","WINWDM Info")!=NULL;
            if (!found) Sleep(100);
        } while (!found && GetTickCount()-start<10000UL);
        fprintf(report,"Info window open before uninstall %s\n",found ? "yes" : "no");
        if (!found) goto done;
    }
    sprintf(command,"%s /S",registration);
    if (!CreateProcessA(NULL,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) goto done;
    WaitForSingleObject(child.hProcess,INFINITE);
    if (!GetExitCodeProcess(child.hProcess,&exitCode)) exitCode=1;
    CloseHandle(child.hThread);
    CloseHandle(child.hProcess);
    fprintf(report,"Launcher exit %lu\n",exitCode);
    if (exitCode) goto done;
verify:
    /* NSIS may continue in a temporary copy after its first process exits. */
    start=GetTickCount();
    do {
        LONG result=RegOpenKeyExA(HKEY_LOCAL_MACHINE,entry,0,KEY_QUERY_VALUE,&key);
        if (!result) RegCloseKey(key);
        if (result==ERROR_FILE_NOT_FOUND && GetFileAttributesA(info)==INVALID_FILE_ATTRIBUTES) {
            DWORD error=GetLastError();
            if (error==ERROR_FILE_NOT_FOUND || error==ERROR_PATH_NOT_FOUND) {
                removed=!openInfo || (!FindWindowA("WINWDM Info","WINWDM Info") &&
                        GetFileAttributesA(folder)==INVALID_FILE_ATTRIBUTES);
                if (removed) break;
            }
        }
        if (verifyOnly) break;
        Sleep(100);
    } while (GetTickCount()-start<60000UL);
    if (openInfo) {
        BOOL stillOpen=FindWindowA("WINWDM Info","WINWDM Info")!=NULL;
        BOOL hasFolder=GetFileAttributesA(folder)!=INVALID_FILE_ATTRIBUTES;
        fprintf(report,"Info window after removal %s\n",stillOpen ? "present" : "absent");
        fprintf(report,"Installation directory after removal %s\n",hasFolder ? "present" : "absent");
        if (stillOpen || hasFolder) removed=FALSE;
    }
done:
    fprintf(report,"Uninstall registration and Info file removal %s\n",removed ? "PASS" : "FAIL");
    fclose(report);
    return removed ? 0 : 1;
}
