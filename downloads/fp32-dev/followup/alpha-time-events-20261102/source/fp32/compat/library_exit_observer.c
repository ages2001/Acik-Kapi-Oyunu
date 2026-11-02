#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static DWORD detached;
DWORD WINAPI GetDetachCount(void){return detached;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,PVOID reserved)
{(void)module;(void)reserved;if(reason==DLL_THREAD_DETACH)detached++;return TRUE;}
