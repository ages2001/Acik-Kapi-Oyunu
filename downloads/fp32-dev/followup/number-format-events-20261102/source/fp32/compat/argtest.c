#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef LPWSTR *(WINAPI *Parse)(LPCWSTR,int *);
static HANDLE logh;
static unsigned passed, failed;
static void check(int ok, const char *name) {
    DWORD w; const char *p = ok ? "PASS " : "FAIL ";
    if (ok) passed++; else failed++;
    WriteFile(logh,p,5,&w,0); WriteFile(logh,name,lstrlenA(name),&w,0);
    WriteFile(logh,"\r\n",2,&w,0); FlushFileBuffers(logh);
}
static int equal(LPCWSTR a,LPCWSTR b) { while(*a && *a==*b) {a++;b++;} return *a==*b; }
static void test(Parse parse,LPCWSTR command,int expected,LPCWSTR a,LPCWSTR b,LPCWSTR c,const char *name) {
    int n=-1,ok; LPWSTR *v=parse(command,&n);
    ok=v && n==expected && v[n]==0;
    if(ok && expected>0)ok=equal(v[0],a);
    if(ok && expected>1)ok=equal(v[1],b);
    if(ok && expected>2)ok=equal(v[2],c);
    check(ok,name);
    if(v)check(LocalFree(v)==0,"result released with LocalFree");
}
void WINAPI entry(void) {
    HMODULE module; Parse parse; LPWSTR *v; int n=-1;
    logh=CreateFileA("C:\\FP32\\ARGTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
    module=LoadLibraryA("C:\\FP32\\F32S.DLL");
    parse=module?(Parse)GetProcAddress(module,"CommandLineToArgvW"):0;
    if(!parse){check(0,"load command-line adapter");ExitProcess(2);}
    test(parse,L"player.exe",1,L"player.exe",0,0,"program only");
    test(parse,L"player.exe game.swf",2,L"player.exe",L"game.swf",0,"simple file");
    test(parse,L"\"C:\\Program Files\\Flash\\player.exe\" \"C:\\My Games\\a.swf\"",2,L"C:\\Program Files\\Flash\\player.exe",L"C:\\My Games\\a.swf",0,"quoted executable and SWF paths");
    test(parse,L"p\tfile\toption",3,L"p",L"file",L"option","tab separators");
    test(parse,L"  file",2,L"",L"file",0,"leading whitespace creates empty argv zero");
    test(parse,L"p   ",1,L"p",0,0,"trailing whitespace");
    test(parse,L"p \"\" x",3,L"p",L"",L"x","empty quoted argument");
    test(parse,L"p ab\"cd ef\"gh",2,L"p",L"abcd efgh",0,"quoted fragment inside argument");
    test(parse,L"p \"unterminated path",2,L"p",L"unterminated path",0,"unclosed quoted argument");
    test(parse,L"p C:\\dir\\",2,L"p",L"C:\\dir\\",0,"unquoted trailing backslash");
    test(parse,L"p \"C:\\dir\\\\\"",2,L"p",L"C:\\dir\\",0,"even backslashes before closing quote");
    test(parse,L"p a\\\"b",2,L"p",L"a\"b",0,"odd backslash escapes quote");
    test(parse,L"p \"a\"\"b\"",2,L"p",L"a\"b",0,"adjacent quotes in quoted argument");
    test(parse,L"p \"\"\"",2,L"p",L"\"",0,"triple quote argument");
    test(parse,L"p \x03a9\x4e2d.swf",2,L"p",L"\x03a9\x4e2d.swf",0,"Unicode arguments preserved");
    v=parse(L"",&n);check(v&&n==1&&v[0][0]&&v[1]==0,"empty command returns executable path");if(v)LocalFree(v);
    SetLastError(0);v=parse(L"p",0);check(!v&&GetLastError()==ERROR_INVALID_PARAMETER,"invalid count pointer rejected");
    SetLastError(0);v=parse(0,&n);check(!v&&GetLastError()==ERROR_INVALID_PARAMETER,"null command rejected");
    CloseHandle(logh);FreeLibrary(module);
    MessageBoxA(0,failed?"Command-line adapter tests failed. See C:\\FP32\\ARGTEST.LOG.":"Command-line adapter: 33 checks passed.","FP32 API test - not the player",MB_OK);
    ExitProcess(failed?1:0);
}
