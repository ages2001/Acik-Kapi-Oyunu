/* Unload even this proxy, then exit without executing any unloaded code. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
__attribute__((noreturn)) void WINAPI fp_FreeLibraryAndExitThread(HMODULE module,DWORD code)
{
    HMODULE kernel=GetModuleHandleA("KERNEL32.DLL");
    FARPROC unload=GetProcAddress(kernel,"FreeLibrary");
    FARPROC exit_thread=GetProcAddress(kernel,"ExitThread");
    /* Native FreeLibrary returns directly into native ExitThread. Its ret 4
       removes the module argument and exposes ExitThread's return/code frame.
       No return address or import thunk in this DLL is used after unloading. */
    __asm__ volatile("pushl %0\n\tpushl $0\n\tpushl %1\n\tpushl %2\n\tjmp *%3"
                     ::"r"(code),"r"(module),"r"(exit_thread),"r"(unload):"memory");
    __builtin_unreachable();
}
