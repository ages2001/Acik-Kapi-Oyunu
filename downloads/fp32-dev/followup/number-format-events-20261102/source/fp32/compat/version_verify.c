/* OSVERSIONINFOEXA's CSD text is not part of VerifyVersionInfo's type mask.
   Preserve numeric fields exactly and reuse the shared condition evaluator. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
BOOL WINAPI fp_VerifyVersionInfoW(LPOSVERSIONINFOEXW,DWORD,ULONGLONG);
BOOL WINAPI fp_VerifyVersionInfoA(LPOSVERSIONINFOEXA input,DWORD types,ULONGLONG conditions)
{
    OSVERSIONINFOEXW wide;
    if(!input || input->dwOSVersionInfoSize!=sizeof(*input)){
        SetLastError(ERROR_INVALID_PARAMETER);return FALSE;
    }
    wide.dwOSVersionInfoSize=sizeof(wide);
    wide.dwMajorVersion=input->dwMajorVersion;
    wide.dwMinorVersion=input->dwMinorVersion;
    wide.dwBuildNumber=input->dwBuildNumber;
    wide.dwPlatformId=input->dwPlatformId;
    wide.szCSDVersion[0]=0;
    wide.wServicePackMajor=input->wServicePackMajor;
    wide.wServicePackMinor=input->wServicePackMinor;
    wide.wSuiteMask=input->wSuiteMask;
    wide.wProductType=input->wProductType;
    wide.wReserved=input->wReserved;
    return fp_VerifyVersionInfoW(&wide,types,conditions);
}
