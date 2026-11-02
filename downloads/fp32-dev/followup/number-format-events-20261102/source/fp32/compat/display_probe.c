/* Read native display capabilities, bypassing all player GDI adapters. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE file;
static void value(const char*name,DWORD value){char line[180];DWORD written;wsprintfA(line,"%s %lu\r\n",name,value);WriteFile(file,line,lstrlenA(line),&written,0);}
void WINAPI entry(void){HDC dc;HKEY key;DWORD bytes,type,written;char path[512];
 file=CreateFileA("C:\\FP32\\DISPLAY.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(file==INVALID_HANDLE_VALUE)ExitProcess(1);
 dc=GetDC(0);value("Native BITSPIXEL",GetDeviceCaps(dc,BITSPIXEL));value("Native PLANES",GetDeviceCaps(dc,PLANES));value("Native NUMCOLORS",GetDeviceCaps(dc,NUMCOLORS));value("Native HORZRES",GetDeviceCaps(dc,HORZRES));value("Native VERTRES",GetDeviceCaps(dc,VERTRES));ReleaseDC(0,dc);
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"HARDWARE\\DEVICEMAP\\VIDEO",0,KEY_READ,&key)==0){bytes=sizeof(path);if(RegQueryValueExA(key,"\\Device\\Video0",0,&type,(BYTE*)path,&bytes)==0&&type==REG_SZ&&bytes<sizeof(path)){path[bytes]=0;WriteFile(file,path,lstrlenA(path),&written,0);}RegCloseKey(key);}
 CloseHandle(file);ExitProcess(0);
}
