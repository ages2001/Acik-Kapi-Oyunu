/* Read-only integration evidence: actual loader paths in the live player.
   NT3.x x86 PEB/LDR layout only; never write to the observed process. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static HANDLE process,logh;static DWORD failures;static BYTE buffer[4096];
static BOOL read_at(DWORD address,void*output,DWORD length){SIZE_T got=0;BOOL ok=ReadProcessMemory(process,(void*)address,output,length,&got);if(!ok||got!=length)failures++;return ok&&got==length;}
static void line(const char*text){DWORD n;WriteFile(logh,text,lstrlenA(text),&n,0);FlushFileBuffers(logh);}
void WINAPI entry(void)
{
    typedef LONG(WINAPI*Query)(HANDLE,ULONG,void*,ULONG,ULONG*);
    Query query=(void*)GetProcAddress(GetModuleHandleA("NTDLL.DLL"),"NtQueryInformationProcess");
    DWORD basic[6],loader,head,node,entry[13],pid=0,length=0,count=0,crc,n,i,j;WCHAR wide[MAX_PATH];char path[MAX_PATH],text[400];HANDLE file;
    HWND window=FindWindowA("ShockwaveFlash",0);
    logh=CreateFileA("C:\\FP32\\MODULES.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(logh==INVALID_HANDLE_VALUE)ExitProcess(1);
    if(window)GetWindowThreadProcessId(window,&pid);
    process=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ,FALSE,pid);
    if(!process||!query){failures++;goto done;}
    if(query(process,0,basic,sizeof(basic),&length)<0){failures++;goto done;}
    if(!read_at(basic[1]+12,&loader,4))goto done;
    head=loader+12;if(!read_at(head,&node,4))goto done;
    while(node!=head&&count<128){
        if(!read_at(node,entry,sizeof(entry)))break;
        length=(USHORT)entry[9];if(length>=sizeof(wide)||length%2){failures++;break;}
        if(!read_at(entry[10],wide,length))break;wide[length/2]=0;
        if(!WideCharToMultiByte(CP_ACP,0,wide,-1,path,sizeof(path),0,0)){failures++;break;}
        wsprintfA(text,"%08lX %s\r\n",entry[6],path);line(text);count++;
        /* Check actual loaded DLL files in the application root. The large
           original executable has a separately recorded immutable digest. */
        if(((path[0]=='D'||path[0]=='d')&&path[1]==':'&&path[2]=='\\')||((path[0]=='C'||path[0]=='c')&&path[1]==':'&&path[2]=='\\'&&(path[3]=='F'||path[3]=='f')&&(path[4]=='P'||path[4]=='p')&&path[5]=='3'&&path[6]=='2'&&path[7]=='\\')){
            n=lstrlenA(path);
            if(n>=4&&!lstrcmpiA(path+n-4,".DLL")){
                file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);crc=~0u;length=0;
                if(file==INVALID_HANDLE_VALUE){failures++;break;}
                for(;;){if(!ReadFile(file,buffer,sizeof(buffer),&n,0)){failures++;break;}if(!n)break;length+=n;
                    for(i=0;i<n;i++){crc^=buffer[i];for(j=0;j<8;j++)crc=(crc>>1)^(0xedb88320u&-(LONG)(crc&1));}}
                CloseHandle(file);wsprintfA(text,"CRC %08lX %lu %s\r\n",crc^~0u,length,path);line(text);
            }
        }
        node=entry[0];
    }
    if(count==128)failures++;
done:
    wsprintfA(text,"Player PID %lu\r\nModules %lu\r\nFailures %lu\r\n",pid,count,failures);line(text);if(process)CloseHandle(process);CloseHandle(logh);ExitProcess(failures?2:0);
}
