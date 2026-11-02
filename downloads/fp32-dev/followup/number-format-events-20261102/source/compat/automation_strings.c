/* The two Automation string operations imported by the original player.
   Prefer native OLEAUT32 where present so other native COM components share
   its allocator. NT 3.1 without Automation uses the documented BSTR layout. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef WCHAR *String;
typedef String(WINAPI *Allocate)(LPCWSTR);
typedef void(WINAPI *Release)(String);
static Allocate native_allocate;
static Release native_release;
static HMODULE native_module;
static volatile LONG initialized,guard;
static void initialize(void){HMODULE m;Allocate a=0;Release r=0;BOOL keep=FALSE;if(initialized)return;
 /* Never hold our lock across loader calls: another thread can enter this
    function while owning the loader lock. Publish the complete pair together. */
 m=LoadLibraryA("OLEAUT32.DLL");if(m){a=(Allocate)GetProcAddress(m,"SysAllocString");r=(Release)GetProcAddress(m,"SysFreeString");}
 while(InterlockedExchange(&guard,1))Sleep(0);if(!initialized){if(a&&r){native_module=m;native_allocate=a;native_release=r;keep=TRUE;}initialized=1;}InterlockedExchange(&guard,0);if(m&&!keep)FreeLibrary(m);}

String WINAPI fp_SysAllocString(LPCWSTR source){DWORD length=0,bytes,i,*block;String result;initialize();if(native_allocate)return native_allocate(source);if(!source)return 0;while(source[length]){if(length>=0x7ffffffc)return 0;length++;}bytes=length*sizeof(WCHAR);block=HeapAlloc(GetProcessHeap(),0,bytes+sizeof(DWORD)+sizeof(WCHAR));if(!block)return 0;*block=bytes;result=(String)(block+1);for(i=0;i<=length;i++)result[i]=source[i];return result;}
void WINAPI fp_SysFreeString(String value){initialize();if(native_release){native_release(value);return;}if(value)HeapFree(GetProcessHeap(),0,((DWORD*)value)-1);}
