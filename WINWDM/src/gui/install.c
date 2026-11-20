#include "kit.h"
#include <direct.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

extern BOOL WkGroup(HWND,const char *,BOOL);
#ifdef _WIN32
#define KERNEL_FILE "WINWDM.SYS"
#else
#define KERNEL_FILE "WINWDM.386"
#endif
static const char *files[]={KERNEL_FILE,"INFO.EXE","UNINST.EXE"};
static const char marker[]="WINWDM software driver 1\r\n";

static BOOL error(HWND w,const char *msg)
{
    MessageBox(w,msg,KIT_TITLE,MB_ICONEXCLAMATION|MB_OK); return FALSE;
}
static BOOL paths(char *dir,char *driver,char *manifest)
{
    int n=GetWindowsDirectory(dir,PATH_CAP);
    if (!n || n>PATH_CAP-32) return FALSE;
    strcat(dir,"\\WINWDM");
    strcpy(driver,dir); strcat(driver,"\\" KERNEL_FILE);
    strcpy(manifest,dir); strcat(manifest,"\\KIT.ID");
    return TRUE;
}
static BOOL copy(const char *source,const char *target)
{
    FILE *in=fopen(source,"rb"), *out;
    char data[2048]; size_t count; BOOL ok=TRUE;
    if (!in) return FALSE;
    out=fopen(target,"wb");
    if (!out) { fclose(in); return FALSE; }
    while ((count=fread(data,1,sizeof(data),in))>0) {
        if (fwrite(data,1,count,out)!=count) { ok=FALSE; break; }
    }
    if (ferror(in)) ok=FALSE;
    fclose(in);
    if (fclose(out)) ok=FALSE;
    if (!ok) remove(target);
    return ok;
}
static BOOL owned(const char *manifest)
{
    FILE *f=fopen(manifest,"rb"); char buffer[sizeof(marker)];
    size_t n;
    if (!f) return FALSE;
    n=fread(buffer,1,sizeof(buffer),f); fclose(f);
    return n==sizeof(marker)-1 && !memcmp(buffer,marker,n);
}
static BOOL emptyFolder(const char *dir)
{
    char pattern[PATH_CAP];
    struct find_t item;
    unsigned attr, result;
    if (_dos_getfileattr(dir,&attr) || !(attr & _A_SUBDIR)) return FALSE;
    wsprintf(pattern,"%s\\*.*",dir);
    result=_dos_findfirst(pattern,_A_RDONLY|_A_HIDDEN|_A_SYSTEM|_A_SUBDIR|_A_ARCH,&item);
    if (result) return result==18 || result==2;
    do {
        if (strcmp(item.name,".") && strcmp(item.name,"..")) {
            _dos_findclose(&item); return FALSE;
        }
        result=_dos_findnext(&item);
    } while (!result);
    _dos_findclose(&item);
    return result==18;
}
static BOOL driverChange(const char *driver,BOOL install)
{
#ifdef _WIN32
    return WkService(driver,install);
#else
    return WkEditVxd(driver,install);
#endif
}
BOOL WkInstall(HWND w, unsigned selection)
{
    char dir[PATH_CAP],driver[PATH_CAP],manifest[PATH_CAP],source[PATH_CAP],to[PATH_CAP];
    char *leaf; FILE *f; int i, written=0; BOOL registered=FALSE, marked=FALSE;
    if (selection>=WK_TARGET_COUNT) return error(w,"Select a WDM version.");
    if (!paths(dir,driver,manifest)) return error(w,"Cannot locate the Windows directory.");
    if (GetWindowsDirectory(to,sizeof(to)) && strlen(to)<PATH_CAP-17) {
        strcat(to,"\\WDMKIT\\KIT.ID");
        if (!access(to,0)) return error(w,"Remove the earlier WDM Compatibility Kit before installing WINWDM.");
    }
    if (owned(manifest)) return error(w,"The kit is already installed. Run Uninstall before installing again.");
#ifndef _WIN32
    if (!(GetWinFlags() & WF_ENHANCED))
        return error(w,"Start Windows in 386 enhanced mode (WIN /3), then run Setup.");
#endif
#ifdef _WIN32
    if (!GetModuleFileName(NULL,source,sizeof(source)))
#else
    if (!GetModuleFileName((HINSTANCE)GetWindowWord(w,GWW_HINSTANCE),source,sizeof(source)))
#endif
        return error(w,"Cannot locate the installation files.");
    leaf=strrchr(source,'\\');
    if (!leaf || leaf-source>PATH_CAP-16) return error(w,"The installation path is too long.");
    if (mkdir(dir) && !emptyFolder(dir))
        return error(w,"Setup needs an empty WINWDM folder under Windows.");
    for (i=0;i<3;++i) {
        if (i==0) wsprintf(leaf+1,"%s%s",WkTargets[selection].stem,strrchr(KERNEL_FILE,'.'));
        else strcpy(leaf+1,files[i]);
        wsprintf(to,"%s\\%s",dir,files[i]);
        if (!copy(source,to)) goto failed;
        ++written;
    }
    f=fopen(manifest,"wb");
    if (!f) goto failed;
    marked=fwrite(marker,1,sizeof(marker)-1,f)==sizeof(marker)-1;
    if (fclose(f) || !marked) goto failed;
    registered=driverChange(driver,TRUE);
    if (!registered || !WkGroup(w,dir,TRUE)) goto failed;
    return TRUE;
failed:
    if (registered) { WkGroup(w,dir,FALSE); if (!driverChange(driver,FALSE))
        return error(w,"Driver rollback failed. The driver files were retained. Run Uninstall to retry."); }
    remove(manifest);
    for (i=0;i<written;++i) { wsprintf(to,"%s\\%s",dir,files[i]); remove(to); }
    rmdir(dir);
    return error(w,"Setup could not finish. Check the source files, Program Manager and write access.");
}

BOOL WkUninstall(HWND w)
{
    char dir[PATH_CAP],driver[PATH_CAP],manifest[PATH_CAP],to[PATH_CAP]; int i;
    if (!paths(dir,driver,manifest) || !owned(manifest))
        return error(w,"Cannot verify the installed kit.");
    if (!driverChange(driver,FALSE)) return error(w,"Could not unregister the driver. Close WINWDM Info and retry.");
    if (!WkGroup(w,dir,FALSE)) return error(w,"Program Manager could not remove the kit group.");
    for (i=0;i<3;++i) {
        wsprintf(to,"%s\\%s",dir,files[i]);
        if (access(to,0)==0 && remove(to)) return error(w,"A kit file is still in use. Close it and retry.");
    }
    if (remove(manifest)) return error(w,"Could not remove the installation marker.");
    /* A Program Manager item may make the kit directory the process working
     * directory. DOS cannot remove a directory while it is current. */
    if (GetWindowsDirectory(to,sizeof(to))) chdir(to);
    rmdir(dir);
    return TRUE;
}

/* Run the removal UI from a temporary copy. The installed EXE can then be
 * deleted as soon as the user confirms removal. */
int WkRemoveLauncher(HINSTANCE app,LPSTR cmd)
{
    char source[PATH_CAP], temporary[PATH_CAP], command[PATH_CAP+16];
#ifdef _WIN32
    char tempdir[PATH_CAP];
    STARTUPINFOA startup;
    PROCESS_INFORMATION child;
    DWORD length, launchError;
#endif
    if (!lstrcmpi(cmd,"/cleanup")) return -1;
    if (!GetModuleFileName(app,source,sizeof(source))) return 1;
#ifdef _WIN32
    length=GetTempPathA(sizeof(tempdir),tempdir);
    if (!length || length>=sizeof(tempdir)-14 || !GetTempFileNameA(tempdir,"WDK",0,temporary)) return 1;
#else
    if (!GetTempFileName(0,"WDK",0,temporary)) return 1;
#endif
    if (!copy(source,temporary)) { remove(temporary); return 1; }
#ifdef _WIN32
    wsprintf(command,"\"%s\" /cleanup",temporary);
    memset(&startup,0,sizeof(startup));
    startup.cb=sizeof(startup);
    /* Give NT 3.1 the exact image name when launching the temporary PE. */
    if (CreateProcessA(temporary,command,NULL,NULL,FALSE,0,NULL,NULL,&startup,&child)) {
        CloseHandle(child.hThread);
        CloseHandle(child.hProcess);
        return 0;
    }
    launchError=GetLastError();
    wsprintf(command,"Could not start the temporary uninstaller. Windows error %lu.",launchError);
#else
    wsprintf(command,"%s /cleanup",temporary);
    if (WinExec(command,SW_SHOW)>31) return 0;
    strcpy(command,"Could not start the temporary uninstaller.");
#endif
    remove(temporary);
    error(NULL,command);
    return 1;
}

void WkCleanLauncher(HINSTANCE app)
{
    char file[PATH_CAP];
    if (!GetModuleFileName(app,file,sizeof(file))) return;
    if (!strrchr(file,'.') || stricmp(strrchr(file,'.'),".TMP")) return;
    if (remove(file)) {
#ifdef _WIN32
        MoveFileExA(file,NULL,MOVEFILE_DELAY_UNTIL_REBOOT);
#else
        char command[PATH_CAP+26];
        wsprintf(command,"COMMAND.COM /C DEL %s",file);
        WinExec(command,SW_HIDE);
#endif
    }
}
