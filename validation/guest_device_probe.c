/* Test-guest device diagnostics and explicit PnP lifecycle operations.
 * Windows 2000 / XP; no CRT, no changes unless requested by command. */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <newdev.h>
static void zero(void *p, unsigned n) { volatile unsigned char *b=p; while(n--)*b++=0; }
static void out(const char *s) { DWORD w; WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,lstrlenA(s),&w,0); }
static void hex(DWORD v) { char b[11]; int i; b[0]='0';b[1]='x';b[10]=0;for(i=0;i<8;i++)b[i+2]="0123456789ABCDEF"[(v>>(28-4*i))&15];out(b); }
static int contains(const char *s,const char *part) {
 int i,j; for(i=0;s[i];i++) { for(j=0;part[j] && s[i+j] && (s[i+j]==part[j]);j++); if(!part[j])return 1; } return 0;
}
void Entry(void) {
 char line[4096],*args[8],*p; int n=0, i; DWORD error=0;
 lstrcpynA(line,GetCommandLineA(),sizeof(line));p=line;
 while(*p && n<8) { while(*p==' ')p++; if(!*p)break; if(*p=='"'){args[n++]=++p;while(*p && *p!='"')p++;}else{args[n++]=p;while(*p && *p!=' ')p++;}if(*p)*p++=0; }
 if(n<2){out("Usage: devprobe list | all | update INF HARDWARE_ID | disable INSTANCE_ID | enable INSTANCE_ID | rescan\r\n");ExitProcess(2);}
 if(!lstrcmpiA(args[1],"update") && n==4) {
  BOOL reboot=FALSE;
  if(!UpdateDriverForPlugAndPlayDevicesA(NULL,args[3],args[2],INSTALLFLAG_FORCE,&reboot))error=GetLastError();
  out("update error=");hex(error);out(" reboot=");hex(reboot);out("\r\n");ExitProcess(error?1:0);
 }
 if(!lstrcmpiA(args[1],"rescan")) { DEVINST root;CONFIGRET cr=CM_Locate_DevNodeA(&root,NULL,CM_LOCATE_DEVNODE_NORMAL);if(!cr)cr=CM_Reenumerate_DevNode(root,CM_REENUMERATE_SYNCHRONOUS);out("rescan status=");hex(cr);out("\r\n");ExitProcess(cr?1:0); }
 {
  HDEVINFO set=SetupDiGetClassDevsA(NULL,NULL,NULL,DIGCF_ALLCLASSES|DIGCF_PRESENT);
  SP_DEVINFO_DATA d;
  if(set==INVALID_HANDLE_VALUE)ExitProcess(3);
  for(i=0;;i++) {
   char id[512],name[256],hardware[2048];ULONG status=0,problem=0;
   zero(&d,sizeof(d));d.cbSize=sizeof(d);
   if(!SetupDiEnumDeviceInfo(set,i,&d))break;
   if(!SetupDiGetDeviceInstanceIdA(set,&d,id,sizeof(id),NULL))continue;
   if(lstrcmpiA(args[1],"all") && !contains(id,"VEN_10EC") && !contains(id,"VID_0BDA"))continue;
   name[0]=0;hardware[0]=0;
   if(!SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_FRIENDLYNAME,NULL,(BYTE*)name,sizeof(name),NULL))
    SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_DEVICEDESC,NULL,(BYTE*)name,sizeof(name),NULL);
   SetupDiGetDeviceRegistryPropertyA(set,&d,SPDRP_HARDWAREID,NULL,(BYTE*)hardware,sizeof(hardware),NULL);
   CM_Get_DevNode_Status(&status,&problem,d.DevInst,0);
   out(id);out("\r\n  name: ");out(name);out("\r\n  hardware: ");out(hardware);
   out("\r\n  status: ");hex(status);out(" problem: ");hex(problem);out("\r\n");
   if(n==3 && !lstrcmpiA(id,args[2]) && (!lstrcmpiA(args[1],"enable") || !lstrcmpiA(args[1],"disable"))) {
    SP_PROPCHANGE_PARAMS change;zero(&change,sizeof(change));change.ClassInstallHeader.cbSize=sizeof(SP_CLASSINSTALL_HEADER);change.ClassInstallHeader.InstallFunction=DIF_PROPERTYCHANGE;
    change.StateChange=!lstrcmpiA(args[1],"enable")?DICS_ENABLE:DICS_DISABLE;change.Scope=DICS_FLAG_GLOBAL;
    if(!SetupDiSetClassInstallParamsA(set,&d,&change.ClassInstallHeader,sizeof(change)) || !SetupDiCallClassInstaller(DIF_PROPERTYCHANGE,set,&d))error=GetLastError();
    out("  change error: ");hex(error);out("\r\n");SetupDiDestroyDeviceInfoList(set);ExitProcess(error?1:0);
   }
  }
  SetupDiDestroyDeviceInfoList(set);
 }
 ExitProcess(lstrcmpiA(args[1],"list") && lstrcmpiA(args[1],"all")?4:0);
}
