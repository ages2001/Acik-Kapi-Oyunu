#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI entry(void) {
    HMODULE dll=LoadLibraryA("FPCPU.DLL");
    void(WINAPI*check)(void)=dll?(void*)GetProcAddress(dll,"FpRequireSse2"):0;
    if(!check)ExitProcess(2);
    check();
    MessageBoxA(0,"SSE2 hardware and operating-system context support checks passed.",
                "CPU gate test - not the player",MB_OK);
    FreeLibrary(dll);ExitProcess(0);
}
