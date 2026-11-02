/* Read-only examination of our own native NT 3.1 test heap. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static char text[16384];static DWORD used;
static void row(const char*name,DWORD a,DWORD b,DWORD c,DWORD d){used+=wsprintfA(text+used,"%s %08lX %08lX %08lX %08lX\r\n",name,a,b,c,d);}
void WINAPI entry(void){typedef LONG(WINAPI *Snap)(HANDLE,LPVOID,DWORD,LPDWORD);Snap snap=(Snap)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"RtlSnapShotHeap");HANDLE heap=HeapCreate(0,4096,0),out;DWORD sizes[4]={1,37,4096,300000};void*blocks[4];DWORD i,j,needed,status,n;BYTE*buf=VirtualAlloc(0,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);if(!heap||!snap||!buf)ExitProcess(1);
 for(i=0;i<4;i++){blocks[i]=HeapAlloc(heap,0,sizes[i]);if(!blocks[i])ExitProcess(2);row("ALLOC",sizes[i],(DWORD)blocks[i],HeapSize(heap,0,blocks[i]),0);for(j=0;j<16;j+=4)row("HEADER",j,*(DWORD*)((BYTE*)blocks[i]-16+j),0,0);}
 status=snap(heap,buf,65536,&needed);row("SNAP",status,needed,*(DWORD*)(buf+8),0);for(i=0;i<0x38;i+=4)row("HEAD",i,*(DWORD*)(buf+i),0,0);if(status==0)for(i=0x38;i+16<=needed&&i<1024;i+=16)row("ENTRY",*(DWORD*)(buf+i),*(DWORD*)(buf+i+4),*(DWORD*)(buf+i+8),*(DWORD*)(buf+i+12));
 for(i=0;i<16;i++){DWORD*s=(DWORD*)*((DWORD*)((BYTE*)heap+0x48+i*4));if(s)row("SEGMENT",(DWORD)s,s[0x24/4],s[0x28/4],s[0x2c/4]);}
 for(i=0;i<4;i++)HeapFree(heap,0,blocks[i]);HeapDestroy(heap);VirtualFree(buf,0,MEM_RELEASE);out=CreateFileA("C:\\FP32\\HEAPSNAP.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(out!=INVALID_HANDLE_VALUE){WriteFile(out,text,used,&n,0);CloseHandle(out);}ExitProcess(0);}
