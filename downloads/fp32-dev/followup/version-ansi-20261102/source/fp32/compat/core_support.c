#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* InterlockedCompareExchange is absent on the earliest systems. */
LONG WINAPI fp_InterlockedCompareExchange(volatile LONG *p,LONG value,LONG expected) {
    LONG old;
    __asm__ volatile("lock cmpxchgl %2,%1":"=a"(old),"+m"(*p):"r"(value),"0"(expected):"memory","cc");
    return old;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) { return TRUE; }
