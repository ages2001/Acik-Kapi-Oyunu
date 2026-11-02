#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;
static BOOL CALLBACK item(LPWSTR text){LCID id=0;WCHAR w[32];char b[100];DWORD n;int i;for(i=0;text[i];i++){WCHAR c=text[i];id=id*16+(c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c-'a'+10);}if(GetLocaleInfoW(id,LOCALE_INEGNUMBER|LOCALE_NOUSEROVERRIDE,w,32)){wsprintfA(b,"%08lX %u\r\n",id,w[0]-'0');WriteFile(logh,b,lstrlenA(b),&n,0);}return TRUE;}
void WINAPI entry(void){BOOL(WINAPI*enumerate)(LOCALE_ENUMPROCW,DWORD)=(void*)GetProcAddress(GetModuleHandleA("KERNEL32.DLL"),"EnumSystemLocalesW");logh=CreateFileA("C:\\FP32\\NLSNEG.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE||!enumerate)ExitProcess(1);enumerate(item,LCID_SUPPORTED);CloseHandle(logh);ExitProcess(0);}
