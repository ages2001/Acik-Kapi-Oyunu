#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void *memcpy(void*destination,const void*source,size_t length){BYTE*d=destination;const BYTE*s=source;while(length--)*d++=*s++;return destination;}
static HANDLE logh;static DWORD checks,failures;
static void value(const char*text,DWORD number){char b[180];DWORD n;wsprintfA(b,"%s %lu\r\n",text,number);WriteFile(logh,b,lstrlenA(b),&n,0);FlushFileBuffers(logh);}
static void check(BOOL ok,const char*name){checks++;if(!ok)failures++;value(name,ok);}
void WINAPI entry(void)
{
    HMODULE dll=0;OSVERSIONINFOEXA have,want,before;OSVERSIONINFOEXW wide;
    BOOL(WINAPI*verify)(LPOSVERSIONINFOEXA,DWORD,ULONGLONG);
    BOOL(WINAPI*verifyw)(LPOSVERSIONINFOEXW,DWORD,ULONGLONG);
    BOOL(WINAPI*get)(LPOSVERSIONINFOA);
    BOOL(WINAPI*getw)(LPOSVERSIONINFOW);
    ULONGLONG(WINAPI*mask)(ULONGLONG,DWORD,BYTE);
    ULONGLONG condition;DWORD types,i,raw=GetVersion();BOOL result,same;
    logh=CreateFileA("C:\\FP32\\VERANSI.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
    dll=LoadLibraryA("C:\\FP32\\V32K.DLL");check(dll!=0,"Production kernel proxy loads");if(!dll)goto done;
    verify=(void*)GetProcAddress(dll,"VerifyVersionInfoA");verifyw=(void*)GetProcAddress(dll,"VerifyVersionInfoW");
    get=(void*)GetProcAddress(dll,"GetVersionExA");getw=(void*)GetProcAddress(dll,"GetVersionExW");mask=(void*)GetProcAddress(dll,"VerSetConditionMask");
    check(verify&&verifyw&&get&&getw&&mask,"Version query and verification exports resolve");if(!verify||!verifyw||!get||!getw||!mask)goto done;
    have.dwOSVersionInfoSize=sizeof(have);wide.dwOSVersionInfoSize=sizeof(wide);
    check(get((LPOSVERSIONINFOA)&have)&&getw((LPOSVERSIONINFOW)&wide),"Actual extended OS version queried");
    check(have.dwMajorVersion==(raw&255)&&have.dwMinorVersion==((raw>>8)&255),"Version agrees with native GetVersion");
    want=have;types=VER_MAJORVERSION|VER_MINORVERSION|VER_BUILDNUMBER|VER_PLATFORMID|VER_SERVICEPACKMAJOR|VER_SERVICEPACKMINOR|VER_PRODUCT_TYPE;
    condition=mask(0,types,VER_EQUAL);
    check(verify(&want,types,condition),"Exact current numeric version matches");
    /* No terminator: CSD text must not be inspected by this API. */
    for(i=0;i<sizeof(want.szCSDVersion);i++)want.szCSDVersion[i]=(char)0xff;
    before=want;result=verify(&want,types,condition);same=TRUE;
    for(i=0;i<sizeof(want);i++)if(((BYTE*)&before)[i]!=((BYTE*)&want)[i])same=FALSE;
    check(result&&same,"Ignored CSD bytes and caller structure remain untouched");
    want=have;want.dwMajorVersion++;
    check(!verify(&want,VER_MAJORVERSION,mask(0,VER_MAJORVERSION,VER_GREATER_EQUAL))&&GetLastError()==ERROR_OLD_WIN_VERSION,"Future major version fails with OLD_WIN_VERSION");
    want=have;want.dwMajorVersion--;want.dwMinorVersion=999;
    check(verify(&want,VER_MAJORVERSION|VER_MINORVERSION,mask(0,VER_MAJORVERSION|VER_MINORVERSION,VER_GREATER_EQUAL)),"Major version takes precedence over minor version");
    want=have;want.dwBuildNumber++;
    check(verify(&want,VER_BUILDNUMBER,mask(0,VER_BUILDNUMBER,VER_LESS)),"Lower current build satisfies LESS");
    want=have;want.dwPlatformId++;
    check(!verify(&want,VER_PLATFORMID,mask(0,VER_PLATFORMID,VER_EQUAL))&&GetLastError()==ERROR_OLD_WIN_VERSION,"Different platform is rejected");
    want=have;want.wServicePackMajor++;
    check(!verify(&want,VER_SERVICEPACKMAJOR,mask(0,VER_SERVICEPACKMAJOR,VER_GREATER_EQUAL))&&GetLastError()==ERROR_OLD_WIN_VERSION,"Future service pack is rejected");
    want=have;want.wProductType=have.wProductType==VER_NT_WORKSTATION?VER_NT_SERVER:VER_NT_WORKSTATION;
    check(!verify(&want,VER_PRODUCT_TYPE,mask(0,VER_PRODUCT_TYPE,VER_EQUAL))&&GetLastError()==ERROR_OLD_WIN_VERSION,"Different product type is rejected");
    want=have;want.wSuiteMask=0;
    check(verify(&want,VER_SUITENAME,mask(0,VER_SUITENAME,VER_AND)),"Empty AND suite requirement succeeds");
    check(!verify(&want,VER_SUITENAME,mask(0,VER_SUITENAME,VER_OR))&&GetLastError()==ERROR_OLD_WIN_VERSION,"Empty OR suite requirement fails");
    want=have;
    check(verify(&want,types,condition)==verifyw(&wide,types,condition),"ANSI and Unicode agree for current version");
    check(!verify(0,types,condition)&&GetLastError()==ERROR_INVALID_PARAMETER,"Null ANSI structure is rejected");
    want.dwOSVersionInfoSize=sizeof(OSVERSIONINFOA);
    check(!verify(&want,types,condition)&&GetLastError()==ERROR_INVALID_PARAMETER,"Nonextended ANSI size is rejected");
    want=have;
    check(!verify(&want,0,condition)&&GetLastError()==ERROR_INVALID_PARAMETER,"Empty type mask is rejected");
    check(!verify(&want,0x100,condition)&&GetLastError()==ERROR_INVALID_PARAMETER,"Unknown type bit is rejected");
    check(!verify(&want,VER_MAJORVERSION,0)&&GetLastError()==ERROR_INVALID_PARAMETER,"Missing comparison operator is rejected");
done:
    if(dll)FreeLibrary(dll);value("Checks",checks);value("Failures",failures);CloseHandle(logh);ExitProcess(failures?2:0);
}
