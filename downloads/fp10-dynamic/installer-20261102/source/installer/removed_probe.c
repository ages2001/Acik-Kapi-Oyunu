/* Read-only post-uninstall check for the C:\F10TEST integration test. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddeml.h>
typedef struct {DWORD magic,created,had_key,had_value,type,size;BYTE previous[4096];} State;
static State state;static BYTE value[4096];static HANDLE logh;static DWORD checks,failures;
static const char *files[]={"FP10.EXE","FPA.DLL","FPC.DLL","FPCLR.DLL","FPD.DLL","FPG.DLL","FPK.DLL","FPM.DLL","FPO.DLL","FPR.DLL","FPRG.DLL","FPS.DLL","FPU.DLL","FPW.DLL","FP10CPU.DLL","INSTALL.DAT"};
static void text(const char*s){DWORD w;WriteFile(logh,s,lstrlenA(s),&w,0);}
static void check(BOOL ok,const char*s){checks++;if(!ok)failures++;text(s);text(ok?": PASS\r\n":": FAIL\r\n");}
static BOOL contains(const char*s,const char*n){DWORD i;for(;*s;s++){for(i=0;n[i]&&s[i]==n[i];i++){}if(!n[i])return TRUE;}return FALSE;}
static HDDEDATA CALLBACK callback(UINT t,UINT f,HCONV c,HSZ a,HSZ b,HDDEDATA d,ULONG_PTR x,ULONG_PTR y){return 0;}
void WINAPI entry(void){DWORD n,i,type,size=sizeof(value),id=0;BOOL equal;HKEY key;HANDLE f;LONG status;char path[260],summary[80];HSZ service,item;HCONV conv;HDDEDATA data;
 logh=CreateFileA("C:\\FP32\\REMCHK.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 f=CreateFileA("C:\\FP32\\F10STATE.DAT",GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
 equal=f!=INVALID_HANDLE_VALUE&&ReadFile(f,&state,sizeof(state),&n,0)&&n==sizeof(state)&&state.magic==0x31545346&&state.had_value&&state.size<=sizeof(value);
 if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);check(equal,"Original association backup read");
 if(equal){status=RegOpenKeyExA(HKEY_CLASSES_ROOT,".swf",0,KEY_READ,&key);if(!status){status=RegQueryValueExA(key,0,0,&type,value,&size);RegCloseKey(key);}equal=!status&&type==state.type&&size==state.size;for(i=0;equal&&i<size;i++)if(value[i]!=state.previous[i])equal=FALSE;check(equal,"Previous SWF association restored byte for byte");}
 status=RegOpenKeyExA(HKEY_CLASSES_ROOT,"Ages2001.FP10.SWF",0,KEY_READ,&key);if(!status)RegCloseKey(key);check(status==ERROR_FILE_NOT_FOUND,"Product class removed");
 for(i=0;i<sizeof(files)/sizeof(files[0]);i++){lstrcpyA(path,"C:\\F10TEST\\");lstrcatA(path,files[i]);SetLastError(0);n=GetFileAttributesA(path);check(n==INVALID_FILE_ATTRIBUTES&&GetLastError()==ERROR_FILE_NOT_FOUND,files[i]);}
 check(GetFileAttributesA("C:\\F10TEST\\MODETEST.EXE")!=INVALID_FILE_ATTRIBUTES,"Unrelated executable preserved");
 check(GetFileAttributesA("C:\\F10TEST\\MODETEST.LOG")!=INVALID_FILE_ATTRIBUTES,"Unrelated log preserved");
 check(DdeInitializeA(&id,callback,APPCMD_CLIENTONLY,0)==DMLERR_NO_ERROR,"DDE initialized");
 if(id){service=DdeCreateStringHandleA(id,"PROGMAN",CP_WINANSI);item=DdeCreateStringHandleA(id,"Groups",CP_WINANSI);conv=DdeConnect(id,service,service,0);check(conv!=0,"Program Manager connected");if(conv){data=DdeClientTransaction(0,0,conv,item,CF_TEXT,XTYP_REQUEST,10000,0);check(data!=0,"Group list read");if(data){size=DdeGetData(data,value,sizeof(value)-1,0);value[size]=0;DdeFreeDataHandle(data);check(!contains((char*)value,"Flash Player 10 - ages2001"),"Player group removed");}DdeDisconnect(conv);}DdeFreeStringHandle(id,item);DdeFreeStringHandle(id,service);DdeUninitialize(id);}
 wsprintfA(summary,"Checks %lu\r\nFailures %lu\r\n",checks,failures);text(summary);CloseHandle(logh);ExitProcess(failures?2:0);
}
