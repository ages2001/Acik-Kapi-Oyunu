/* Provision application-owned storage on OSes without native AppData.
   Keep it outside the managed APP payload; uninstall must preserve UserData. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static BOOL append(WCHAR *out,LPCWSTR tail){DWORD n=lstrlenW(out),k=lstrlenW(tail);if(n+k>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}lstrcpyW(out+n,tail);return TRUE;}
static BOOL directory(LPCWSTR path){DWORD a=GetFileAttributesW(path);if(a!=INVALID_FILE_ATTRIBUTES){if(a&FILE_ATTRIBUTE_DIRECTORY)return TRUE;SetLastError(ERROR_DIRECTORY);return FALSE;}if(CreateDirectoryW(path,0))return TRUE;if(GetLastError()!=ERROR_ALREADY_EXISTS)return FALSE;a=GetFileAttributesW(path);return a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)!=0;}
static BOOL tree(LPCWSTR path){WCHAR part[MAX_PATH];DWORD i,start=3,n=lstrlenW(path);if(n>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}lstrcpyW(part,path);
 if(part[0]=='\\'&&part[1]=='\\'){start=2;while(part[start]&&part[start]!='\\')start++;if(part[start])start++;while(part[start]&&part[start]!='\\')start++;if(part[start])start++;}
 else if(n<3||part[1]!=':'||part[2]!='\\'){SetLastError(ERROR_BAD_PATHNAME);return FALSE;}
 for(i=start;i<n;i++)if(part[i]=='\\'){part[i]=0;if(!directory(part))return FALSE;part[i]='\\';}
 return directory(part);
}
BOOL WINAPI fp_PrepareSaveDirectory(LPCWSTR installation){
 WCHAR path[MAX_PATH],chunk[10];DWORD raw[128],needed,n,i,k;BYTE*sid;HANDLE token;HMODULE shell,adv;
 typedef BOOL(WINAPI *OpenToken)(HANDLE,DWORD,PHANDLE);
 typedef BOOL(WINAPI *TokenInfo)(HANDLE,TOKEN_INFORMATION_CLASS,LPVOID,DWORD,PDWORD);
 typedef DWORD(WINAPI *SidLength)(PSID);
 OpenToken open_token;TokenInfo token_info;SidLength sid_length;
 shell=LoadLibraryA("SHELL32.DLL");if(shell){FARPROC native=GetProcAddress(shell,"SHGetFolderPathW");FreeLibrary(shell);if(native)return TRUE;}
 if(lstrlenW(installation)>=MAX_PATH){SetLastError(ERROR_FILENAME_EXCED_RANGE);return FALSE;}
 lstrcpyW(path,installation);if(!append(path,L"\\UserData"))return FALSE;
 if(GetVersion()&0x80000000u){if(!append(path,L"\\Default"))return FALSE;}
 else {
  adv=LoadLibraryA("ADVAPI32.DLL");if(!adv)return FALSE;
  open_token=(OpenToken)GetProcAddress(adv,"OpenProcessToken");token_info=(TokenInfo)GetProcAddress(adv,"GetTokenInformation");sid_length=(SidLength)GetProcAddress(adv,"GetLengthSid");
  if(!open_token||!token_info||!sid_length){FreeLibrary(adv);SetLastError(ERROR_CALL_NOT_IMPLEMENTED);return FALSE;}
  if(!open_token(GetCurrentProcess(),TOKEN_QUERY,&token)){DWORD e=GetLastError();FreeLibrary(adv);SetLastError(e);return FALSE;}
  if(!token_info(token,TokenUser,raw,sizeof(raw),&needed)){DWORD e=GetLastError();CloseHandle(token);FreeLibrary(adv);SetLastError(e);return FALSE;}
  CloseHandle(token);sid=(BYTE*)((TOKEN_USER*)raw)->User.Sid;n=sid_length(sid);FreeLibrary(adv);
  if(n<8||n>68||sid<(BYTE*)raw||sid+n>(BYTE*)raw+sizeof(raw)){SetLastError(ERROR_INVALID_SID);return FALSE;}
  /* Full SID, losslessly split into 8-character names: no username collision,
     account-rename dependence, or unsupported long directory component. */
  for(i=0;i<n;){chunk[0]='\\';k=1;while(i<n&&k<9){BYTE v=sid[i++];chunk[k++]=L"0123456789ABCDEF"[v>>4];chunk[k++]=L"0123456789ABCDEF"[v&15];}chunk[k]=0;if(!append(path,chunk))return FALSE;}
 }
 if(!tree(path))return FALSE;
 return SetEnvironmentVariableW(L"FP32_USERDATA",path);
}
HRESULT WINAPI fp_LegacyAppDataPath(HWND owner,int folder,HANDLE token,DWORD flags,LPWSTR out){WCHAR path[MAX_PATH];DWORD n,attributes;
 if(!out)return E_POINTER;out[0]=0;
 if(token||flags!=0)return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
 if((folder&0xff)!=0x1a||(folder&~0xc0ff))return E_INVALIDARG;
 n=GetEnvironmentVariableW(L"FP32_USERDATA",path,MAX_PATH);
 if(!n)return HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND);
 if(n>=MAX_PATH)return HRESULT_FROM_WIN32(ERROR_FILENAME_EXCED_RANGE);
 if(folder&0x8000){if(!tree(path))return HRESULT_FROM_WIN32(GetLastError());}
 if(!(folder&0x4000)){attributes=GetFileAttributesW(path);if(attributes==INVALID_FILE_ATTRIBUTES)return HRESULT_FROM_WIN32(GetLastError());if(!(attributes&FILE_ATTRIBUTE_DIRECTORY))return HRESULT_FROM_WIN32(ERROR_DIRECTORY);}
 lstrcpyW(out,path);return S_OK;
}
