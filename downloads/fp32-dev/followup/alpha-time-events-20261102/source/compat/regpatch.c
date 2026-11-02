#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static struct {DWORD offset;BYTE old,new;} changes[]={{352,57,150},{353,143,229},{5862346,65,70},{5862347,68,80},{5862348,86,82},{5862349,65,71},{5862350,80,46},{5862351,73,68},{5862352,51,76},{5862353,50,76},{5862354,46,0},{5862355,100,0},{5862356,108,0},{5862357,108,0}};
void WINAPI entry(void){HANDLE h;DWORD i,n;BYTE b;BOOL ok=TRUE;
if(!CopyFileA("C:\\FP10\\TRACE\\FPTRACE.EXE","C:\\FP10\\TRACE\\FPREG.EXE",TRUE))ExitProcess(1);
h=CreateFileA("C:\\FP10\\TRACE\\FPREG.EXE",GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,0,0);if(h==INVALID_HANDLE_VALUE)ExitProcess(2);
for(i=0;i<sizeof(changes)/sizeof(changes[0]);i++){SetFilePointer(h,changes[i].offset,0,FILE_BEGIN);if(!ReadFile(h,&b,1,&n,0)||n!=1||b!=changes[i].old){ok=FALSE;break;}}
if(ok)for(i=0;i<sizeof(changes)/sizeof(changes[0]);i++){SetFilePointer(h,changes[i].offset,0,FILE_BEGIN);if(!WriteFile(h,&changes[i].new,1,&n,0)||n!=1){ok=FALSE;break;}}
FlushFileBuffers(h);CloseHandle(h);if(!ok)DeleteFileA("C:\\FP10\\TRACE\\FPREG.EXE");MessageBoxA(0,ok?"Registry diagnostic copy prepared.":"Patch failed; diagnostic copy removed.","Development test helper",0);ExitProcess(ok?0:3);}
