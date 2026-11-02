#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
extern void WINAPI FpObserve(const char *,DWORD);
extern HRESULT WINAPI fp_LegacyAppDataPath(HWND,int,HANDLE,DWORD,LPWSTR);
static HRESULT WINAPI folder_bridge(HWND owner,int folder,HANDLE token,DWORD flags,LPWSTR path){
    static DWORD observed;
    if(observed++<8){
    FpObserve("SHGetFolderPathW folder",(DWORD)folder);
    FpObserve("SHGetFolderPathW flags",flags);
    FpObserve("SHGetFolderPathW token",(DWORD)token);
    FpObserve("SHGetFolderPathW caller",(DWORD)__builtin_return_address(0));
    }
    return fp_LegacyAppDataPath(owner,folder,token,flags,path);
}
static void observe_path(LPCWSTR text,const char *kind) {
    char out[196];int i=0;FpObserve(kind,(DWORD)text);if(!text)return;
    while(text[i]&&i<195){out[i]=text[i]<128?(char)text[i]:'?';i++;}out[i]=0;FpObserve(out,0);
}
static BOOL WINAPI legacy_execute(SHELLEXECUTEINFOW *info) {
    typedef HINSTANCE (WINAPI *RealExecute)(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,LPWSTR,LPCWSTR,LPVOID,INT,LPHANDLE);
    HMODULE shell;RealExecute execute;HINSTANCE result;
    if(!info||info->cbSize!=sizeof(*info)){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
    info->hProcess=0;
    observe_path(info->lpVerb,"ShellExecuteExW verb");observe_path(info->lpFile,"ShellExecuteExW file");observe_path(info->lpParameters,"ShellExecuteExW parameters");FpObserve("ShellExecuteExW mask",info->fMask);
    if(info->fMask&~(SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC)){SetLastError(ERROR_NOT_SUPPORTED);info->hInstApp=(HINSTANCE)SE_ERR_ACCESSDENIED;return FALSE;}
    shell=GetModuleHandleA("SHELL32.DLL");
    execute=shell?(RealExecute)GetProcAddress(shell,"RealShellExecuteW"):0;
    if(!execute){SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
    /* The native NT 3.1 export has ten stdcall parameters; its last parameter
       receives the real process handle. Its own ShellExecuteW wrapper supplies
       NULL there. Keep native association, DDE and executable-launch behavior. */
    result=execute(info->hwnd,info->lpVerb,info->lpFile,info->lpParameters?info->lpParameters:L"",info->lpDirectory,0,L"",0,info->nShow,(info->fMask&SEE_MASK_NOCLOSEPROCESS)?&info->hProcess:0);
    info->hInstApp=result;
    FpObserve("ShellExecuteExW result",(DWORD)result);
    if((UINT_PTR)result>32)return TRUE;
    SetLastError((DWORD)(UINT_PTR)result);return FALSE;
}
FARPROC WINAPI fp_GetProcAddress(HMODULE module,LPCSTR name) {
    static DWORD missing_observations;
    FARPROC result=GetProcAddress(module,name);DWORD error=GetLastError();
    if(!result&&(UINT_PTR)name>65535&&missing_observations++<64){FpObserve("Optional API unavailable",(DWORD)module);FpObserve(name,0);}
    if(!result&&(UINT_PTR)name>65535&&module==GetModuleHandleA("SHELL32.DLL")&&!lstrcmpA(name,"ShellExecuteExW"))return (FARPROC)legacy_execute;
    if(!result&&(UINT_PTR)name>65535&&module==GetModuleHandleA("SHELL32.DLL")&&!lstrcmpA(name,"SHGetFolderPathW"))return (FARPROC)folder_bridge;
    /* The original engine also resolves its RNG dynamically for save IDs.
       Route these lookups to the same real provider used by static imports. */
    if(!result&&(UINT_PTR)name>65535&&module==GetModuleHandleA("ADVAPI32.DLL")&&
       (!lstrcmpA(name,"CryptAcquireContextA")||!lstrcmpA(name,"CryptAcquireContextW")||
        !lstrcmpA(name,"CryptGenRandom")||!lstrcmpA(name,"CryptReleaseContext"))){
        HMODULE proxy=GetModuleHandleA("F32A.DLL");
        if(proxy){result=GetProcAddress(proxy,name);if(result)return result;}
    }
    SetLastError(error);return result;
}
