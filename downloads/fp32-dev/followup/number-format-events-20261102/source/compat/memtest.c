#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE logh;
static void value(const char*s,DWORD v){char b[160];DWORD i=0,j,n;const char*h="0123456789ABCDEF";while(*s)b[i++]=*s++;b[i++]=' ';for(j=0;j<8;j++)b[i++]=h[(v>>(28-j*4))&15];b[i++]='\r';b[i++]='\n';WriteFile(logh,b,i,&n,0);}
void WINAPI entry(void){MEMORYSTATUS m;DWORD start=GetTickCount();m.dwLength=sizeof(m);GlobalMemoryStatus(&m);logh=CreateFileA("C:\\FP10\\MEMORY.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);value("PhysicalTotal",m.dwTotalPhys);value("PhysicalAvailable",m.dwAvailPhys);value("PageFileTotal",m.dwTotalPageFile);value("PageFileAvailable",m.dwAvailPageFile);value("MemoryLoadPercent",m.dwMemoryLoad);Sleep(500);value("Sleep500Elapsed",GetTickCount()-start);FlushFileBuffers(logh);CloseHandle(logh);ExitProcess(0);}
