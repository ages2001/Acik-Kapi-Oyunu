/* Read-only native snapshot of the installed player's private save tree. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static char path[MAX_PATH]="C:\\FP32\\UserData",report[32768];static BYTE buffer[4096];static DWORD used,files,errors;
static void walk(void){WIN32_FIND_DATAA fd;HANDLE search,file;DWORD len=lstrlenA(path),n,count,crc,i,k,ok,w;char suffix[MAX_PATH];
 if(len>MAX_PATH-15){errors++;return;}lstrcpyA(path+len,"\\*.*");search=FindFirstFileA(path,&fd);path[len]=0;if(search==INVALID_HANDLE_VALUE){errors++;return;}
 do{if(!lstrcmpA(fd.cFileName,".")||!lstrcmpA(fd.cFileName,".."))continue;
  if(len+lstrlenA(fd.cFileName)+2>=MAX_PATH){errors++;continue;}path[len]='\\';lstrcpyA(path+len+1,fd.cFileName);
  if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)walk();
  else{file=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);if(file==INVALID_HANDLE_VALUE){errors++;path[len]=0;continue;}count=0;crc=~0u;
   for(;;){ok=ReadFile(file,buffer,sizeof(buffer),&n,0);if(!ok){errors++;break;}if(!n)break;count+=n;for(i=0;i<n;i++){crc^=buffer[i];for(k=0;k<8;k++)crc=(crc>>1)^(0xedb88320u&-(LONG)(crc&1));}}
   CloseHandle(file);if(used+MAX_PATH+80>=sizeof(report)){errors++;break;}used+=wsprintfA(report+used,"%lu %08lX %s\r\n",count,crc^~0u,path);files++;
  }path[len]=0;
 }while(FindNextFileA(search,&fd));FindClose(search);path[len]=0;
}
void WINAPI entry(void){HANDLE f;DWORD n;char *s=GetCommandLineA();
 /* Optional quoted save-tree root; the existing installed-player default
    remains useful for uninstall/reinstall tests. Never modify save files. */
 if(*s=='"'){s++;while(*s&&*s!='"')s++;if(*s)s++;}
 else while(*s&&*s!=' '&&*s!='\t')s++;
 while(*s==' '||*s=='\t')s++;
 if(*s){BOOL quoted=*s=='"';if(quoted)s++;n=0;
  while(*s&&(!quoted||*s!='"')){if(n>=MAX_PATH-1)ExitProcess(2);path[n++]=*s++;}
  path[n]=0;if(!n)ExitProcess(2);
 }
 walk();used+=wsprintfA(report+used,"Files: %lu\r\nErrors: %lu\r\n",files,errors);f=CreateFileA("C:\\FP32\\SAVETREE.LOG",GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,0,0);if(f!=INVALID_HANDLE_VALUE){WriteFile(f,report,used,&n,0);CloseHandle(f);}ExitProcess(errors?1:0);}
