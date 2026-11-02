#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;static int fails,checks;
static void logline(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(logh,s,n,&w,0);WriteFile(logh,"\r\n",2,&w,0);}
static void hexlog(DWORD n){char s[9];int i;const char*h="0123456789ABCDEF";for(i=0;i<8;i++)s[i]=h[(n>>(28-i*4))&15];s[8]=0;logline(s);}
static void check(int ok,const char*s){checks++;if(!ok){fails++;logline("FAIL");}else logline("PASS");logline(s);}
void WINAPI entry(void){
 HMODULE m=LoadLibraryA("C:\\FP10\\TRACE\\FPK.DLL");HANDLE f;DWORD written;LARGE_INTEGER v;volatile LONG x=5;FILETIME ft;SYSTEMTIME st;OSVERSIONINFOA version;CRITICAL_SECTION cs;
 void(WINAPI *timefn)(LPFILETIME);BOOL(WINAPI *sizefn)(HANDLE,PLARGE_INTEGER);BOOL(WINAPI *seekfn)(HANDLE,LARGE_INTEGER,PLARGE_INTEGER,DWORD);LONG(WINAPI *casfn)(volatile LONG*,LONG,LONG);LONG(WINAPI *addfn)(volatile LONG*,LONG);BOOL(WINAPI *csfn)(LPCRITICAL_SECTION,DWORD);BOOL(WINAPI *verfn)(LPOSVERSIONINFOA);BOOL(WINAPI *featurefn)(DWORD);
 logh=CreateFileA("C:\\FP10\\KTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(!m){logline("FAILED TO LOAD FPK");ExitProcess(2);}
 timefn=(void*)GetProcAddress(m,"GetSystemTimeAsFileTime");sizefn=(void*)GetProcAddress(m,"GetFileSizeEx");seekfn=(void*)GetProcAddress(m,"SetFilePointerEx");casfn=(void*)GetProcAddress(m,"InterlockedCompareExchange");addfn=(void*)GetProcAddress(m,"InterlockedExchangeAdd");csfn=(void*)GetProcAddress(m,"InitializeCriticalSectionAndSpinCount");verfn=(void*)GetProcAddress(m,"GetVersionExA");featurefn=(void*)GetProcAddress(m,"IsProcessorFeaturePresent");
 if(!timefn||!sizefn||!seekfn||!casfn||!addfn||!csfn||!verfn||!featurefn){logline("MISSING EXPORT");ExitProcess(3);}
 timefn(&ft);check(FileTimeToSystemTime(&ft,&st)&&st.wYear>=2026&&st.wYear<2100,"FILETIME converts to current calendar year");
 f=CreateFileA("C:\\FP10\\KTEST.DAT",GENERIC_READ|GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 check(f!=INVALID_HANDLE_VALUE,"create test file");WriteFile(f,"test",4,&written,0);check(sizefn(f,&v)&&v.LowPart==4&&v.HighPart==0,"file size is four bytes");
 v.QuadPart=2;check(seekfn(f,v,&v,FILE_BEGIN)&&v.QuadPart==2,"seek to byte two");v.QuadPart=-1;check(!seekfn(f,v,0,FILE_BEGIN),"negative absolute seek fails");CloseHandle(f);check(!sizefn(INVALID_HANDLE_VALUE,&v),"invalid file handle fails");
 check(casfn(&x,9,5)==5&&x==9,"compare exchange success");check(casfn(&x,3,5)==9&&x==9,"compare exchange mismatch");check(addfn(&x,7)==9&&x==16,"exchange add old and new values");
 check(csfn(&cs,1000),"critical section initialization");EnterCriticalSection(&cs);LeaveCriticalSection(&cs);DeleteCriticalSection(&cs);
 version.dwOSVersionInfoSize=sizeof(version);verfn(&version);logline("version raw/major/minor/build");hexlog(GetVersion());hexlog(version.dwMajorVersion);hexlog(version.dwMinorVersion);hexlog(version.dwBuildNumber);check(verfn(&version)&&version.dwMajorVersion==3&&version.dwMinorVersion==10&&version.dwBuildNumber==HIWORD(GetVersion()),"native NT 3.1 version");
 {LPWCH(WINAPI *envfn)(void)=(void*)GetProcAddress(m,"GetEnvironmentStringsW");BOOL(WINAPI *freefn)(LPWCH)=(void*)GetProcAddress(m,"FreeEnvironmentStringsW");LPWCH env=envfn(),cur;WCHAR expected[256];int found=0,i,j;GetEnvironmentVariableW(L"SystemRoot",expected,256);if(env){for(cur=env;*cur;){const char *name="systemroot=";for(i=0;name[i];i++){WCHAR c=cur[i];if(c>='A'&&c<='Z')c+=32;if(c!=name[i])break;}if(!name[i]){j=0;while(expected[j]&&cur[i+j]==expected[j])j++;if(!expected[j]&&!cur[i+j])found=1;}while(*cur)cur++;cur++;}}check(found,"Unicode environment matches native SystemRoot");check(env&&freefn(env),"free Unicode environment snapshot");}
 check(!featurefn(PF_XMMI_INSTRUCTIONS_AVAILABLE)&&!featurefn(PF_XMMI64_INSTRUCTIONS_AVAILABLE),"SSE and SSE2 unavailable");
 logline(fails?"RESULT FAILED":"RESULT PASSED");CloseHandle(logh);MessageBoxA(0,fails?"Kernel adapter checks FAILED; see KTEST.LOG":"14 kernel adapter checks PASSED","NT 3.1 compatibility tests",0);ExitProcess(fails?1:0);
}
