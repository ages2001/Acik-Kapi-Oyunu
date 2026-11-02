/* Disposable NT3.1 integration guest only. Preserve the original BOOT.INI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
void WINAPI entry(void){DWORD attrs,error=0,written;HANDLE log;char text[160];
 attrs=GetFileAttributesA("C:\\BOOT.INI");if(attrs==INVALID_FILE_ATTRIBUTES)error=GetLastError();
 if(!error&&GetFileAttributesA("C:\\FP32\\BVGAOLD.INI")==INVALID_FILE_ATTRIBUTES&&!CopyFileA("C:\\BOOT.INI","C:\\FP32\\BVGAOLD.INI",TRUE))error=GetLastError();
 if(!error){if(!SetFileAttributesA("C:\\BOOT.INI",FILE_ATTRIBUTE_NORMAL))error=GetLastError();
  if(!error&&!CopyFileA("C:\\FP32\\BOOTVGA.INI","C:\\BOOT.INI",FALSE))error=GetLastError();
  if(!SetFileAttributesA("C:\\BOOT.INI",attrs)&&!error)error=GetLastError();}
 log=CreateFileA("C:\\FP32\\BOOTVGA.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(log!=INVALID_HANDLE_VALUE){wsprintfA(text,"BOOT.INI backup and VGA test selection error %lu\r\n",error);WriteFile(log,text,lstrlenA(text),&written,0);CloseHandle(log);}ExitProcess(error?1:0);
}
