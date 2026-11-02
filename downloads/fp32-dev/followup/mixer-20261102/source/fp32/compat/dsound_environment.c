#define WIN32_LEAN_AND_MEAN
#include <windows.h>
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*reserved){return TRUE;}
static FARPROC native(const char *name){return GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),name);}
static LPCH environment(void){typedef LPCH(WINAPI*F)(void);F f=(void*)native("GetEnvironmentStrings");return f?f():0;}
LPWCH WINAPI fp_GetEnvironmentStringsW(void)
{
 typedef LPWCH(WINAPI*F)(void);F f=(void*)native("GetEnvironmentStringsW");LPCH a;LPWCH w;DWORD n=0;int count;
 if(f)return f();a=environment();if(!a)return 0;while(a[n]||a[n+1])n++;n+=2;
 count=MultiByteToWideChar(CP_ACP,0,a,n,0,0);if(!count)return 0;
 w=HeapAlloc(GetProcessHeap(),0,count*sizeof(WCHAR));if(!w){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
 if(!MultiByteToWideChar(CP_ACP,0,a,n,w,count)){HeapFree(GetProcessHeap(),0,w);return 0;}return w;
}
BOOL WINAPI fp_FreeEnvironmentStringsW(LPWCH value)
{
 typedef BOOL(WINAPI*F)(LPWCH);F f=(void*)native("FreeEnvironmentStringsW");
 if(f)return f(value);if(!value){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}return HeapFree(GetProcessHeap(),0,value);
}
BOOL WINAPI fp_FreeEnvironmentStringsA(LPCH value)
{
 typedef BOOL(WINAPI*F)(LPCH);F f=(void*)native("FreeEnvironmentStringsA");
 if(f)return f(value);if(value&&value==environment())return TRUE;SetLastError(ERROR_INVALID_PARAMETER);return FALSE;
}
