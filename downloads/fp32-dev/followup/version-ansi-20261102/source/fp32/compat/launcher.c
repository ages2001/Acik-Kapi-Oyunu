/* Launch the original player from its isolated APP directory, resolving local
   file arguments before changing the child's DLL search working directory. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
LPWSTR *WINAPI fp_CommandLineToArgvW(LPCWSTR,int*);
BOOL WINAPI fp_PrepareSaveDirectory(LPCWSTR);
static WCHAR root[MAX_PATH],player[MAX_PATH],file[MAX_PATH],working[MAX_PATH],command[2048];
static void fail(LPCWSTR message,DWORD code){WCHAR text[512];wsprintfW(text,L"%s\r\n\r\nError code: %lu",message,code);MessageBoxW(0,text,L"Flash Player 32",MB_OK|MB_ICONSTOP);ExitProcess(code?code:1);}
static DWORD length(LPCWSTR s){DWORD n=0;while(s[n])n++;return n;}
static void copy(LPWSTR d,LPCWSTR s){while((*d++=*s++));}
static DWORD quote(LPWSTR out,LPCWSTR input){DWORD n=0,slashes=0;out[n++]='"';for(;;){WCHAR c=*input++;if(c=='\\'){slashes++;continue;}if(c=='"'||!c){while(slashes){out[n++]='\\';out[n++]='\\';slashes--;}if(!c)break;out[n++]='\\';out[n++]=c;}else{while(slashes){out[n++]='\\';slashes--;}out[n++]=c;}}out[n++]='"';out[n]=0;return n;}
void WINAPI entry(void){DWORD n,last,i,code;int argc;LPWSTR*argv;LPCWSTR target=0;BOOL url=FALSE;STARTUPINFOW si;PROCESS_INFORMATION pi;
 n=GetModuleFileNameW(0,root,MAX_PATH);if(!n||n>=MAX_PATH)fail(L"Cannot locate the Flash Player installation.",GetLastError());
 last=n;while(last&&root[last]!='\\')last--;if(!last||last+14>=MAX_PATH)fail(L"The installation path is too long.",ERROR_FILENAME_EXCED_RANGE);root[last]=0;copy(working,root);if(last==2&&root[1]==':')copy(working+last,L"\\");
 copy(player,root);copy(player+last,L"\\APP\\FP32.EXE");
 argv=fp_CommandLineToArgvW(GetCommandLineW(),&argc);if(!argv)fail(L"Cannot read the file argument.",GetLastError());
 if(argc>2)fail(L"Open one SWF file at a time.",ERROR_INVALID_PARAMETER);
 if(argc==2){
  if(!argv[1][0]||length(argv[1])>=MAX_PATH)fail(L"The SWF path is empty or too long.",ERROR_FILENAME_EXCED_RANGE);
  for(i=0;argv[1][i];i++)if(argv[1][i]==':'&&argv[1][i+1]=='/'&&argv[1][i+2]=='/'){url=TRUE;break;}
  if(url)target=argv[1];
  else {n=GetFullPathNameW(argv[1],MAX_PATH,file,0);if(!n||n>=MAX_PATH)fail(L"Cannot resolve the SWF path.",GetLastError());target=file;}
 }
 if(!fp_PrepareSaveDirectory(root))fail(L"Cannot prepare the per-user saved-game folder for saved games.",GetLastError());
 n=quote(command,player);if(target){command[n++]=' ';quote(command+n,target);}
 for(i=0;i<sizeof(si);i++)((BYTE*)&si)[i]=0;for(i=0;i<sizeof(pi);i++)((BYTE*)&pi)[i]=0;si.cb=sizeof(si);
 if(!CreateProcessW(player,command,0,0,FALSE,0,0,working,&si,&pi)){code=GetLastError();LocalFree(argv);fail(L"Cannot start Flash Player. Keep APP\\FP32.EXE and its compatibility DLLs together in the installation.",code);}
 LocalFree(argv);CloseHandle(pi.hThread);CloseHandle(pi.hProcess);ExitProcess(0);
}
