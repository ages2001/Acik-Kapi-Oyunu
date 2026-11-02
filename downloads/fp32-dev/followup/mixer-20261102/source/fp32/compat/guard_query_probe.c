/* Native status and malformed-request checks; no SIMD instructions used. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../../compat/cpu_features.h"
static HANDLE logh;
static void value(const char *s,DWORD n){char b[160],d[12];DWORD i=0,j=0,w;while(*s)b[i++]=*s++;do{d[j++]='0'+n%10;n/=10;}while(n);while(j)b[i++]=d[--j];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&w,0);}
void WINAPI entry(void){HANDLE h;FpGuardStatus s={0};DWORD bytes=0,error,checks=0,failures=0;BOOL ok;
 logh=CreateFileA("C:\\FP32\\GUARDQ.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);
 if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
 value("Hardware SSE2 ",(fp_cpu_edx()&(1u<<26))!=0);
 value("Context ready ",fp_xmm_context_ready());
 h=CreateFileA("\\\\.\\FpXmmGuard",0,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);
 if(h==INVALID_HANDLE_VALUE){value("Device absent error ",GetLastError());CloseHandle(logh);ExitProcess(2);}
 ok=DeviceIoControl(h,FP_GUARD_IOCTL,0,0,&s,sizeof(s),&bytes,0);
 checks++;if(!ok || !fp_guard_status_valid(&s,bytes))failures++;
 value("Query success ",ok);value("Bytes ",bytes);value("Processors ",s.processors);value("Flags ",s.flags);value("Kernel timestamp ",s.kernel_timestamp);
 bytes=123;ok=DeviceIoControl(h,FP_GUARD_IOCTL,0,0,&s,4,&bytes,0);error=GetLastError();checks++;if(ok||error!=ERROR_INSUFFICIENT_BUFFER)failures++;value("Short buffer error ",error);
 ok=DeviceIoControl(h,FP_GUARD_IOCTL,&s,4,&s,sizeof(s),&bytes,0);error=GetLastError();checks++;if(ok||error!=ERROR_INVALID_PARAMETER)failures++;value("Unexpected input error ",error);
 ok=DeviceIoControl(h,FP_GUARD_IOCTL+4,0,0,&s,sizeof(s),&bytes,0);error=GetLastError();checks++;if(ok||error!=ERROR_INVALID_FUNCTION)failures++;value("Unknown request error ",error);
 CloseHandle(h);value("Checks ",checks);value("Failures ",failures);CloseHandle(logh);ExitProcess(failures?3:0);
}
