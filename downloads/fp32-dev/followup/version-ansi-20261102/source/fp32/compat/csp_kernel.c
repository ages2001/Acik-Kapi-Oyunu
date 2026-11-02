#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* NT 3.1 has no thread-callout disabling export. Leaving notifications enabled
   is valid: the supplied CSP's DllMain accepts every notification. */
BOOL WINAPI fp_DisableThreadLibraryCalls(HMODULE module)
{
    (void)module;
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void *reserved)
{
    (void)module; (void)reason; (void)reserved;
    return TRUE;
}
