#define WIN32_LEAN_AND_MEAN
#include <windows.h>

BOOL WINAPI wx_version_info(OSVERSIONINFOW *info);

/* Apply the version contract only to the verified Microsoft executable's
 * minimum-version check. Its subsequent requests, including the System page
 * and saved report, get the running system's unmodified version and build. */
BOOL WINAPI wx_dxdiag_version_w(OSVERSIONINFOW *info)
{
    if (!wx_version_info(info)) return FALSE;
#ifdef WX_VERSION_CHECK_RETURN_RVA
    const BYTE *program = (const BYTE *)GetModuleHandleA(NULL);
    if ((const BYTE *)__builtin_return_address(0) == program + WX_VERSION_CHECK_RETURN_RVA &&
            info->dwMajorVersion < 5) {
        info->dwMajorVersion = 5;
        info->dwMinorVersion = 0;
    }
#endif
    return TRUE;
}
