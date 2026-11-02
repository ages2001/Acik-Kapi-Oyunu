#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static char report[2400];
static DWORD used, passed, failed;
static void check(const char *name, BOOL ok) {
    const char *prefix = ok ? "PASS " : "FAIL ";
    if (ok) passed++; else failed++;
    while (*prefix) report[used++] = *prefix++;
    while (*name) report[used++] = *name++;
    report[used++] = '\r'; report[used++] = '\n'; report[used] = 0;
}
void WINAPI entry(void) {
    HANDLE (WINAPI *openw)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
    DWORD (WINAPI *attrw)(LPCWSTR);
    BOOL (WINAPI *deletew)(LPCWSTR);
    HMODULE dll = LoadLibraryA("F32FILE.DLL");
    HANDLE h, other, log;
    DWORD count, err;
    char readback[4];
    static char payload[] = "FP32";
    static WCHAR path[] = L"C:\\F32IO.TMP";
    static WCHAR lossy[] = { 'C',':','\\','F','3','2',0xd83d,0xde00,'.','T','M','P',0 };
    openw = dll ? (void*)GetProcAddress(dll,"CreateFileW") : 0;
    attrw = dll ? (void*)GetProcAddress(dll,"GetFileAttributesW") : 0;
    deletew = dll ? (void*)GetProcAddress(dll,"DeleteFileW") : 0;
    check("adapter exports", openw && attrw && deletew);
    if (openw && attrw && deletew) {
        /* CREATE_NEW never overwrites a preexisting user file. */
        h = openw(path,GENERIC_READ|GENERIC_WRITE,0,0,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,0);
        check("create new Unicode path", h != INVALID_HANDLE_VALUE);
        if (h != INVALID_HANDLE_VALUE) {
            check("write bytes", WriteFile(h,payload,4,&count,0) && count==4);
            check("seek", SetFilePointer(h,0,0,FILE_BEGIN)==0);
            check("read exact bytes", ReadFile(h,readback,4,&count,0) && count==4 &&
                readback[0]=='F' && readback[1]=='P' && readback[2]=='3' && readback[3]=='2');
            other = openw(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
            err = GetLastError();
            check("deny incompatible sharing", other==INVALID_HANDLE_VALUE && err==ERROR_SHARING_VIOLATION);
            if(other!=INVALID_HANDLE_VALUE) CloseHandle(other);
            CloseHandle(h);
            check("attributes", attrw(path)!=INVALID_FILE_ATTRIBUTES);
            other = openw(path,GENERIC_WRITE,0,0,CREATE_NEW,0,0);
            err = GetLastError();
            check("CREATE_NEW preserves existing file", other==INVALID_HANDLE_VALUE && (err==ERROR_FILE_EXISTS || err==ERROR_ALREADY_EXISTS));
            if(other!=INVALID_HANDLE_VALUE) CloseHandle(other);
            check("delete", deletew(path));
            other = openw(path,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
            err=GetLastError();
            check("missing file error", other==INVALID_HANDLE_VALUE && err==ERROR_FILE_NOT_FOUND);
            if(other!=INVALID_HANDLE_VALUE) CloseHandle(other);
        }
        other = openw(lossy,GENERIC_WRITE,0,0,CREATE_NEW,0,0);
        err = GetLastError();
        check("reject unrepresentable name", other==INVALID_HANDLE_VALUE && err==ERROR_NO_UNICODE_TRANSLATION);
        if(other!=INVALID_HANDLE_VALUE) CloseHandle(other);
        other = openw(0,GENERIC_READ,0,0,OPEN_EXISTING,0,0);
        err = GetLastError();
        check("null path error", other==INVALID_HANDLE_VALUE && err==ERROR_INVALID_PARAMETER);
    }
    log=CreateFileA("C:\\F32FILE.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    if(log!=INVALID_HANDLE_VALUE){WriteFile(log,report,used,&count,0);CloseHandle(log);}
    MessageBoxA(0,report,failed?"FP32 file adapter: FAILED (not player)":"FP32 file adapter: PASSED (not player)",MB_OK);
    if(dll)FreeLibrary(dll);
    ExitProcess(failed?1:0);
}
