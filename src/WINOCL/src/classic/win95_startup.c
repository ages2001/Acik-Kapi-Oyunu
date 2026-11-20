/* Windows 95 rejects LoadLibrary on DLLs with a PE static TLS directory.
 * These GCC builds use Win32 TLS slots, with only the MinGW alignment word
 * in .tls. Deliver the CRT and winpthreads callbacks from the DLL entry. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern const PIMAGE_TLS_CALLBACK __xl_a[], __xl_z[];
extern char _tls_start, _tls_end;
extern const IMAGE_TLS_DIRECTORY32 _wx_tls_template;
extern BOOL WINAPI wx_mingw_entry(HINSTANCE, DWORD, void *);

BOOL WINAPI DllMainCRTStartup(HINSTANCE instance, DWORD reason, void *reserved)
{
    const PIMAGE_TLS_CALLBACK *callback;
    ULONG_PTR first = (ULONG_PTR)&_tls_start + sizeof(void *);
    ULONG_PTR end = (ULONG_PTR)&_tls_end;
    /* MinGW.org places the TLS directory itself between the two sentinels.
     * Count that metadata separately and reject DLLs with static TLS data. */
    if (end != first && ((ULONG_PTR)&_wx_tls_template != first ||
                          end != first + sizeof(_wx_tls_template)))
        return FALSE;
    for (callback = __xl_a + 1; callback < __xl_z; ++callback)
        if (*callback) (*callback)(instance, reason, reserved);
    return wx_mingw_entry(instance, reason, reserved);
}
