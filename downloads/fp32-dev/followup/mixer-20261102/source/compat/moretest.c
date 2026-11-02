#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
static HANDLE logfile;static int total,failed;
static void text(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(logfile,s,n,&w,0);}
static void hexval(DWORD v){char s[9];int i;const char*h="0123456789ABCDEF";for(i=0;i<8;i++)s[i]=h[(v>>(28-i*4))&15];s[8]=0;text(s);text("\r\n");}
static void check(int ok,const char*s){total++;if(!ok)failed++;text(ok?"PASS ":"FAIL ");text(s);text("\r\n");}
#define LOAD(mod,ret,name,args) typedef ret(WINAPI*T_##name)args;T_##name name=(T_##name)GetProcAddress(mod,#name);if(!name){check(0,"missing " #name);return;}
static void locale_tests(HMODULE m){char out[32];WORD ct[2];
 LOAD(m,int,GetLocaleInfoA,(LCID,LCTYPE,LPSTR,int));
 LOAD(m,int,CompareStringA,(LCID,DWORD,LPCSTR,int,LPCSTR,int));
 LOAD(m,int,LCMapStringA,(LCID,DWORD,LPCSTR,int,LPSTR,int));
 LOAD(m,BOOL,GetStringTypeA,(LCID,DWORD,LPCSTR,int,LPWORD));
 check(GetLocaleInfoA(0x409,LOCALE_SDECIMAL,out,32)==2&&out[0]=='.'&&out[1]==0,"locale decimal separator");
 check(CompareStringA(0x409,NORM_IGNORECASE,"ABC",-1,"abc",-1)==CSTR_EQUAL,"case-insensitive comparison");
 check(LCMapStringA(0x409,LCMAP_UPPERCASE,"abc",-1,out,32)==4&&out[0]=='A'&&out[1]=='B'&&out[2]=='C'&&!out[3],"uppercase mapping");
 check(GetStringTypeA(0x409,CT_CTYPE1,"A1",2,ct)&&(ct[0]&C1_ALPHA)&&(ct[1]&C1_DIGIT),"character classification");
 check(LCMapStringA(0x41f,LCMAP_UPPERCASE,"i",-1,out,32)==2&&(BYTE)out[0]==0xdd,"Turkish dotted uppercase I");
 {WCHAR buf[32];CPINFO cp;int r;SetLastError(0);r=GetLocaleInfoW(0x41f,LOCALE_IDEFAULTANSICODEPAGE,buf,32);text("DIAG Turkish ANSI locale result/error/first char\r\n");hexval(r);hexval(GetLastError());hexval(r?buf[0]:0);SetLastError(0);r=GetCPInfo(1254,&cp);text("DIAG codepage 1254 result/error\r\n");hexval(r);hexval(GetLastError());SetLastError(0);r=LCMapStringW(0x41f,LCMAP_UPPERCASE,L"i",-1,buf,32);text("DIAG native Turkish uppercase result/error/char\r\n");hexval(r);hexval(GetLastError());hexval(r?buf[0]:0);}
 check(!GetLocaleInfoA(0x409,LOCALE_SDECIMAL,out,-1),"negative output capacity rejected");
}
static void version_tests(HMODULE m){OSVERSIONINFOEXW v;ULONGLONG mask;WIN32_FILE_ATTRIBUTE_DATA a;
 LOAD(m,ULONGLONG,VerSetConditionMask,(ULONGLONG,DWORD,BYTE));
 LOAD(m,BOOL,VerifyVersionInfoW,(LPOSVERSIONINFOEXW,DWORD,ULONGLONG));
 LOAD(m,BOOL,GetFileAttributesExW,(LPCWSTR,GET_FILEEX_INFO_LEVELS,LPVOID));
 v.dwOSVersionInfoSize=sizeof(v);v.dwMajorVersion=3;v.dwMinorVersion=10;mask=VerSetConditionMask(0,VER_MAJORVERSION|VER_MINORVERSION,VER_GREATER_EQUAL);
 check(VerifyVersionInfoW(&v,VER_MAJORVERSION|VER_MINORVERSION,mask),"NT 3.10 version predicate");
 v.dwMajorVersion=5;check(!VerifyVersionInfoW(&v,VER_MAJORVERSION,mask)&&GetLastError()==ERROR_OLD_WIN_VERSION,"newer OS predicate fails honestly");
 v.dwMajorVersion=2;v.dwMinorVersion=99;check(VerifyVersionInfoW(&v,VER_MAJORVERSION|VER_MINORVERSION,mask),"version hierarchy ignores lower minor after higher major");
 check(GetFileAttributesExW(L"C:\\FP10\\KTEST.DAT",GetFileExInfoStandard,&a)&&a.nFileSizeLow==4,"extended file attributes and size");
 check(GetFileAttributesExW(L"C:\\",GetFileExInfoStandard,&a)&&(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY),"root directory attributes");
 check(!GetFileAttributesExW(L"C:\\FP10\\ABSENT.XYZ",GetFileExInfoStandard,&a),"missing file fails");
}
static void ole_tests(HMODULE m){void*obj;void*mem;static const GUID absent={0x4c44d51d,0xa511,0x4a00,{0xab,0x21,1,2,3,4,5,6}};static const GUID unknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
 LOAD(m,HRESULT,CoInitialize,(LPVOID));LOAD(m,void,CoUninitialize,(void));LOAD(m,HRESULT,OleInitialize,(LPVOID));LOAD(m,void,OleUninitialize,(void));LOAD(m,LPVOID,CoTaskMemAlloc,(SIZE_T));LOAD(m,void,CoTaskMemFree,(LPVOID));LOAD(m,HRESULT,CoCreateInstance,(REFCLSID,LPUNKNOWN,DWORD,REFIID,LPVOID*));
 obj=(void*)1;check(CoCreateInstance(&absent,0,CLSCTX_INPROC_SERVER,&unknown,&obj)==CO_E_NOTINITIALIZED&&!obj,"COM requires per-thread initialization");
 check(CoInitialize(0)==S_OK,"first COM initialization");check(CoInitialize(0)==S_FALSE,"nested COM initialization");
 obj=(void*)1;check(CoCreateInstance(&absent,0,CLSCTX_INPROC_SERVER,&unknown,&obj)==REGDB_E_CLASSNOTREG&&!obj,"unregistered class fails without fake object");
 mem=CoTaskMemAlloc(64);check(mem!=0,"COM task allocation");if(mem){*(DWORD*)mem=0x12345678;check(*(DWORD*)mem==0x12345678,"COM memory writable");}CoTaskMemFree(mem);
 CoUninitialize();CoUninitialize();check(CoInitialize(0)==S_OK,"balanced COM release resets apartment");
 check(OleInitialize(0)==S_OK&&OleInitialize(0)==S_FALSE,"nested OLE initialization");OleUninitialize();OleUninitialize();CoUninitialize();
}
static LRESULT CALLBACK wndproc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcW(h,m,w,l);}
static void user_tests(HMODULE m){WNDCLASSEXW c,got;HWND w;ATOM atom;MONITORINFO monitor;HMONITOR mon;HINSTANCE instance=GetModuleHandleA(0);
 LOAD(m,ATOM,RegisterClassExW,(const WNDCLASSEXW*));LOAD(m,BOOL,GetClassInfoExW,(HINSTANCE,LPCWSTR,LPWNDCLASSEXW));LOAD(m,HMONITOR,MonitorFromWindow,(HWND,DWORD));LOAD(m,BOOL,GetMonitorInfoW,(HMONITOR,LPMONITORINFO));
 c.cbSize=sizeof(c);c.style=0;c.lpfnWndProc=wndproc;c.cbClsExtra=0;c.cbWndExtra=8;c.hInstance=instance;c.hIcon=LoadIconA(0,IDI_APPLICATION);c.hCursor=0;c.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);c.lpszMenuName=0;c.lpszClassName=L"FPCompatProbe";c.hIconSm=0;
 atom=RegisterClassExW(&c);check(atom!=0,"extended window class registration");got.cbSize=sizeof(got);check(GetClassInfoExW(instance,c.lpszClassName,&got)&&got.cbWndExtra==8&&got.hIconSm==0,"extended class information round trip");
 w=CreateWindowExW(0,c.lpszClassName,L"Compatibility Probe",WS_OVERLAPPEDWINDOW,20,20,320,240,0,0,instance,0);check(w!=0,"native window created from extended class");mon=MonitorFromWindow(w,MONITOR_DEFAULTTOPRIMARY);monitor.cbSize=sizeof(monitor);check(GetMonitorInfoW(mon,&monitor)&&monitor.rcMonitor.right==GetSystemMetrics(SM_CXSCREEN)&&monitor.rcMonitor.bottom==GetSystemMetrics(SM_CYSCREEN),"primary monitor dimensions");if(w)DestroyWindow(w);check(UnregisterClassW(c.lpszClassName,instance),"window class cleanup");
}
void WINAPI entry(void){HMODULE k,o,u;char msg[100];const char*s;int n=0,v,i;char d[12];logfile=CreateFileA("C:\\FP10\\MORETEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);k=LoadLibraryA("C:\\FP10\\TRACE\\FPK.DLL");o=LoadLibraryA("C:\\FP10\\TRACE\\FPO.DLL");u=LoadLibraryA("C:\\FP10\\TRACE\\FPU.DLL");check(k&&o&&u,"adapter DLLs loaded");if(k){locale_tests(k);version_tests(k);}if(o)ole_tests(o);if(u)user_tests(u);s="Checks: ";while(*s)msg[n++]=*s++;v=total;i=0;do{d[i++]='0'+v%10;v/=10;}while(v);while(i)msg[n++]=d[--i];s="; failures: ";while(*s)msg[n++]=*s++;v=failed;i=0;do{d[i++]='0'+v%10;v/=10;}while(v);while(i)msg[n++]=d[--i];msg[n]=0;text(msg);text("\r\n");CloseHandle(logfile);MessageBoxA(0,msg,"NT 3.1 extended compatibility tests",0);ExitProcess(failed?1:0);}
