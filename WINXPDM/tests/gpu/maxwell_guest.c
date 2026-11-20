/* Check the stock and edited NVIDIA INFs with the native XP SetupAPI.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <windows.h>
#include <setupapi.h>

#ifdef _WIN64
#define MODELS "NVIDIA_Devices.NTamd64.5.1"
#else
#define MODELS "NVIDIA_Devices.NTx86.5.1"
#endif

static const char *const device_ids[]={"1401","13C2","13C0","17C8","1B80","1B06"};

static void record(HANDLE log,const char *line)
{
    DWORD size;
    WriteFile(log,line,lstrlenA(line),&size,NULL);
    FlushFileBuffers(log);
}

static BOOL inspect(const char *path,const char *label,BOOL edited,HANDLE log)
{
    INFCONTEXT line,next,svc;
    char device[256],model[256],section[256],actual[256],services[270],driver[256],buf[1024];
    unsigned i,seen=0;
    BOOL ok=TRUE;
    HINF inf=SetupOpenInfFileA(path,NULL,INF_STYLE_WIN4,NULL);
    if (inf==INVALID_HANDLE_VALUE) {
        wsprintfA(buf,"%s open failed %lu\r\n",label,GetLastError());
        record(log,buf);
        return FALSE;
    }
    if (SetupFindFirstLineA(inf,MODELS,NULL,&line)) {
        do {
            if (!SetupGetStringFieldA(&line,2,device,sizeof(device),NULL)) continue;
            for (i=0;i<sizeof(device_ids)/sizeof(device_ids[0]);++i) {
                char id[32];
                wsprintfA(id,"PCI\\VEN_10DE&DEV_%s",device_ids[i]);
                if (lstrcmpiA(device,id)) continue;
                if (!SetupGetStringFieldA(&line,0,model,sizeof(model),NULL) ||
                    !SetupGetStringFieldA(&line,1,section,sizeof(section),NULL) ||
                    !SetupDiGetActualSectionToInstallA(inf,section,actual,sizeof(actual),NULL,NULL)) {
                    ok=FALSE; continue;
                }
                wsprintfA(services,"%s.Services",actual);
                if (!SetupFindFirstLineA(inf,services,"AddService",&svc) ||
                    !SetupGetStringFieldA(&svc,1,driver,sizeof(driver),NULL)) {
                    ok=FALSE; continue;
                }
                wsprintfA(buf,"%s %s %s %s service=%s\r\n",label,id,model,actual,driver);
                record(log,buf);
                if (lstrcmpiA(actual,"Section008") || lstrcmpiA(driver,"nv")) ok=FALSE;
                seen|=1U<<i;
            }
        } while (SetupFindNextLine(&line,&next) && (line=next,TRUE));
    } else ok=FALSE;
    SetupCloseInfFile(inf);
    for (i=0;i<sizeof(device_ids)/sizeof(device_ids[0]);++i) {
        if (!(seen & (1U<<i))) {
            wsprintfA(buf,"%s PCI\\VEN_10DE&DEV_%s no desktop match\r\n",label,device_ids[i]);
            record(log,buf);
        }
    }
    return ok && seen==(edited?15U:1U);
}

int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR args,int show)
{
    char path[MAX_PATH],*name;
    HANDLE log;
    BOOL stock,edited;
    DWORD n=GetModuleFileNameA(NULL,path,sizeof(path));
    (void)instance; (void)previous; (void)args; (void)show;
    if (!n || n>=MAX_PATH-12) return 2;
    for (name=path+n;name>path && name[-1]!='\\';--name) {}
    lstrcpyA(name,"INFTEST.TXT");
    log=CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,0,NULL);
    if (log==INVALID_HANDLE_VALUE) return 2;
    record(log,MODELS "\r\n");
    lstrcpyA(name,"STOCK.INF"); stock=inspect(path,"stock",FALSE,log);
    lstrcpyA(name,"MAXWELL.INF"); edited=inspect(path,"edited",TRUE,log);
    record(log,stock && edited?"result: PASS\r\n":"result: FAIL\r\n");
    CloseHandle(log);
    return stock && edited?0:1;
}
