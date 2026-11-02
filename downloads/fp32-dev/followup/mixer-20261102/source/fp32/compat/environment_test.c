/* Native regression; changes only this process's test environment variables. */
#include "../../compat/kernel.c"
void WINAPI FpObserve(const char *s,DWORD n){(void)s;(void)n;}
void WINAPI FpReport(const char *s,DWORD n){(void)s;(void)n;}
static DWORD checks,errors,used;static char report[4096];
static void check(BOOL ok,const char *name){checks++;if(!ok){errors++;used+=wsprintfA(report+used,"FAIL: %s (error %lu)\r\n",name,GetLastError());}}
static const WCHAR *lookup(const WCHAR *block,const WCHAR *name){
 DWORD i,n=lstrlenW(name);const WCHAR *p=block;if(!p)return 0;
 while(*p){for(i=0;i<n&&p[i]==name[i];i++){}if(i==n&&p[i]=='=')return p+i+1;p+=lstrlenW(p)+1;}return 0;
}
static BOOL equal(const WCHAR *a,const WCHAR *b){if(!a||!b)return FALSE;while(*a&&*a==*b){a++;b++;}return *a==*b;}
void WINAPI entry(void){
 const WCHAR *name=L"AGES_FP_ENV_SNAPSHOT",*unicodeName=L"AGES_FP_ENV_\x4e2d";
 const WCHAR *value=L"C:\\Data\\\x4e2d\\\x03a9",*changed=L"changed";
 WCHAR native[128];WCHAR *first=0,*second=0,*third=0;DWORD i,n;HANDLE file;
 check(SetEnvironmentVariableW(name,value),"set Unicode value");
 check(SetEnvironmentVariableW(unicodeName,L"Unicode name"),"set Unicode variable name");
 n=GetEnvironmentVariableW(name,native,128);
 check(n&&equal(native,value),"native Unicode control");
 used+=wsprintfA(report+used,"Native test-value length: %lu; code units:",n);
 for(i=0;i<n&&i<64;i++)used+=wsprintfA(report+used," %04X",native[i]);
 used+=wsprintfA(report+used,"\r\n");
 first=fp_GetEnvironmentStringsW();check(first!=0,"first snapshot");
 check(equal(lookup(first,name),value),"Unicode value survives snapshot");
 check(equal(lookup(first,unicodeName),L"Unicode name"),"Unicode name survives snapshot");
 check(SetEnvironmentVariableW(name,changed),"change process environment");
 second=fp_GetEnvironmentStringsW();check(second&&second!=first,"independent snapshot allocation");
 check(equal(lookup(first,name),value),"old snapshot remains unchanged");
 check(equal(lookup(second,name),changed),"new snapshot sees update");
 check(SetEnvironmentVariableW(name,0),"delete test variable");
 third=fp_GetEnvironmentStringsW();check(third&&!lookup(third,name),"deleted variable absent");
 check(equal(lookup(second,name),changed),"snapshot survives environment deletion");
 if(first)check(fp_FreeEnvironmentStringsW(first),"free first snapshot");
 if(second)check(fp_FreeEnvironmentStringsW(second),"free second snapshot");
 if(third)check(fp_FreeEnvironmentStringsW(third),"free third snapshot");
 for(i=0;i<32;i++){first=fp_GetEnvironmentStringsW();check(first&&fp_FreeEnvironmentStringsW(first),"repeated snapshot/free");}
 SetEnvironmentVariableW(unicodeName,0);
 check(!fp_FreeEnvironmentStringsW(0)&&GetLastError()==ERROR_INVALID_PARAMETER,"reject NULL snapshot");
 used+=wsprintfA(report+used,"Checks: %lu\r\nErrors: %lu\r\n",checks,errors);
 file=CreateFileA("C:\\FP32\\ENVTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(file!=INVALID_HANDLE_VALUE){WriteFile(file,report,used,&n,0);CloseHandle(file);}ExitProcess(errors?1:0);
}
