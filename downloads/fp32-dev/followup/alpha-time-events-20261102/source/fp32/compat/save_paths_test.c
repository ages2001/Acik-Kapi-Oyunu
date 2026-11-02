/* Exercise private storage through the actual DLL on the guest filesystem. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
typedef DWORD(WINAPI *LongPath)(LPCWSTR,LPWSTR,DWORD);
typedef BOOL(WINAPI *Dir)(LPCWSTR,LPSECURITY_ATTRIBUTES);
typedef BOOL(WINAPI *Remove)(LPCWSTR);
typedef HANDLE(WINAPI *Open)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE);
typedef DWORD(WINAPI *Attrs)(LPCWSTR);
typedef HANDLE(WINAPI *First)(LPCWSTR,LPWIN32_FIND_DATAW);
typedef BOOL(WINAPI *Next)(HANDLE,LPWIN32_FIND_DATAW);
typedef BOOL(WINAPI *Close)(HANDLE);
typedef BOOL(WINAPI *Copy)(LPCWSTR,LPCWSTR,BOOL);
typedef BOOL(WINAPI *Move)(LPCWSTR,LPCWSTR,DWORD);
static DWORD count,errors,used;static char text[4096];
static void check(BOOL ok){DWORD e=GetLastError();count++;if(!ok){errors++;used+=wsprintfA(text+used,"FAIL %lu error %lu\r\n",count,e);}}
static BOOL equal(const BYTE*a,const BYTE*b,DWORD n){DWORD i;for(i=0;i<n;i++)if(a[i]!=b[i])return FALSE;return TRUE;}
void WINAPI entry(void){HMODULE m=LoadLibraryA("D:\\F32K.DLL");LongPath longpath=(LongPath)GetProcAddress(m,"GetLongPathNameW");Dir dir=(Dir)GetProcAddress(m,"CreateDirectoryW");Remove del=(Remove)GetProcAddress(m,"DeleteFileW"),remove=(Remove)GetProcAddress(m,"RemoveDirectoryW");Open open=(Open)GetProcAddress(m,"CreateFileW");Attrs attrs=(Attrs)GetProcAddress(m,"GetFileAttributesW");First first=(First)GetProcAddress(m,"FindFirstFileW");Next next=(Next)GetProcAddress(m,"FindNextFileW");Close close=(Close)GetProcAddress(m,"FindClose");Copy copy=(Copy)GetProcAddress(m,"CopyFileW");Move move=(Move)GetProcAddress(m,"MoveFileExW");static WCHAR root[MAX_PATH],folder[MAX_PATH],a[MAX_PATH],b[MAX_PATH],c[MAX_PATH],pattern[MAX_PATH],extended[MAX_PATH],canonical[MAX_PATH];WIN32_FIND_DATAW fd;HANDLE h,out;DWORD i,n,magic;BYTE bytes[32],one=1,two=2;WCHAR alias_one[14];static const BYTE payload[]="Persistent save payload";LPCWSTR leaf=L"Unicode \x03a9 saved game with a long name.sol";
 check(m&&longpath&&dir&&del&&remove&&open&&attrs&&first&&next&&close&&copy&&move);if(errors)goto done;
 for(i=1;i<100;i++){wsprintfW(root,L"C:\\FP32\\MTEST%03lu",i);if(CreateDirectoryW(root,0))break;}check(i<100);if(i>=100)goto done;check(SetEnvironmentVariableW(L"FP32_USERDATA",root));
 lstrcpyW(folder,root);lstrcatW(folder,L"\\Long directory name for saved games");check(dir(folder,0));lstrcpyW(a,folder);lstrcatW(a,L"\\");lstrcatW(a,leaf);h=open(a,GENERIC_WRITE,0,0,CREATE_NEW,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(WriteFile(h,payload,sizeof(payload),&n,0)&&n==sizeof(payload));check(FlushFileBuffers(h));CloseHandle(h);}
 h=open(a,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(ReadFile(h,bytes,sizeof(bytes),&n,0)&&n==sizeof(payload)&&equal(bytes,payload,n));CloseHandle(h);}check(attrs(a)!=INVALID_FILE_ATTRIBUTES);
 lstrcpyW(pattern,folder);lstrcatW(pattern,L"\\*.sol");h=first(pattern,&fd);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(!lstrcmpW(fd.cFileName,leaf));check(fd.nFileSizeLow==sizeof(payload));check(!next(h,&fd)&&GetLastError()==ERROR_NO_MORE_FILES);check(close(h));}
 lstrcpyW(extended,L"\\\\?\\");lstrcatW(extended,a);h=first(extended,&fd);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(!lstrcmpW(fd.cFileName,leaf));close(h);}lstrcpyW(extended,L"\\\\?\\");lstrcatW(extended,pattern);h=first(extended,&fd);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE)close(h);check(longpath(a,0,0)==(DWORD)lstrlenW(a)+1);canonical[0]='!';check(longpath(a,canonical,1)==(DWORD)lstrlenW(a)+1&&canonical[0]=='!');check(longpath(a,canonical,MAX_PATH)==(DWORD)lstrlenW(a)&&!lstrcmpiW(a,canonical));
 lstrcpyW(b,folder);lstrcatW(b,L"\\Renamed saved game.sol");check(move(a,b,0));check(attrs(a)==INVALID_FILE_ATTRIBUTES);lstrcpyW(c,folder);lstrcatW(c,L"\\Copied saved game.sol");check(copy(b,c,TRUE));check(!copy(b,c,TRUE));check(!remove(folder)&&GetLastError()==ERROR_DIR_NOT_EMPTY);check(del(b));check(del(c));check(remove(folder));
 /* Deliberate FNV-1a/UTF-16 hash collision: each logical object must keep its
    own contents and physical alias, even though both start at B4A594A8. */
 lstrcpyW(a,root);lstrcatW(a,L"\\Save xWFuA2w4bwaWmH.sol");lstrcpyW(b,root);lstrcatW(b,L"\\Save KDuOopEq9pxpGr.sol");
 h=open(a,GENERIC_WRITE,0,0,CREATE_NEW,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(WriteFile(h,&one,1,&n,0)&&n==1);CloseHandle(h);}
 h=open(b,GENERIC_WRITE,0,0,CREATE_NEW,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(WriteFile(h,&two,1,&n,0)&&n==1);CloseHandle(h);}
 h=first(a,&fd);check(h!=INVALID_HANDLE_VALUE);alias_one[0]=0;if(h!=INVALID_HANDLE_VALUE){lstrcpyW(alias_one,fd.cAlternateFileName);close(h);}h=first(b,&fd);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(lstrcmpW(alias_one,fd.cAlternateFileName)!=0);close(h);}
 h=open(a,GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(ReadFile(h,bytes,1,&n,0)&&n==1&&bytes[0]==one);CloseHandle(h);}h=open(b,GENERIC_READ,0,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){check(ReadFile(h,bytes,1,&n,0)&&n==1&&bytes[0]==two);CloseHandle(h);}
 /* Corrupt this test's own map temporarily. Enumeration must report corruption,
    never silently make the still-present save files disappear. */
 lstrcpyW(c,root);lstrcatW(c,L"\\FP32MAP.DAT");h=CreateFileW(c,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){magic=0;check(WriteFile(h,&magic,4,&n,0)&&n==4);CloseHandle(h);lstrcpyW(pattern,root);lstrcatW(pattern,L"\\*.sol");out=first(pattern,&fd);check(out==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_INVALID_DATA);if(out!=INVALID_HANDLE_VALUE)close(out);h=CreateFileW(c,GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);check(h!=INVALID_HANDLE_VALUE);if(h!=INVALID_HANDLE_VALUE){magic=0x314d5046;check(WriteFile(h,&magic,4,&n,0)&&n==4);CloseHandle(h);}}
 check(del(a));check(del(b));check(remove(root));SetEnvironmentVariableW(L"FP32_USERDATA",0);
 done:used+=wsprintfA(text+used,"Checks: %lu\r\nErrors: %lu\r\n",count,errors);out=CreateFileA("C:\\FP32\\MAPTEST.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(out!=INVALID_HANDLE_VALUE){WriteFile(out,text,used,&n,0);CloseHandle(out);}ExitProcess(errors?1:0);}
