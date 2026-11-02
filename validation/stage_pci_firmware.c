/* Guest test fixture: verify a specific binary on the read-only test CD,
 * copy it into the test directory and append an OS-managed delayed rename.
 * This is not the product installer. /verify checks the installed bytes.
 */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#ifndef FIXTURE_SIZE
#error Define the exact fixture size, CRC32 and filename at build time
#endif
static char source[MAX_PATH],target[MAX_PATH];
static unsigned char buffer[4096];
static void out(const char*s){DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);}
static int append(char*path,const char*tail)
{
    unsigned i=0,j=0;while(path[i])i++;
    while(tail[j]){if(i>=MAX_PATH-1)return 0;path[i++]=tail[j++];}path[i]=0;return 1;
}
static int valid(const char*path)
{
    HANDLE f;DWORD size,n,crc=0xffffffffU,total=0,i,j;
    f=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_EXISTING,0,0);
    if(f==INVALID_HANDLE_VALUE)return 0;
    size=GetFileSize(f,0);if(size!=FIXTURE_SIZE){CloseHandle(f);return 0;}
    for(;;){
        if(!ReadFile(f,buffer,sizeof(buffer),&n,0)){CloseHandle(f);return 0;}
        if(!n)break;
        total+=n;if(total>FIXTURE_SIZE){CloseHandle(f);return 0;}
        for(i=0;i<n;i++){crc^=buffer[i];for(j=0;j<8;j++)crc=(crc>>1)^((crc&1)?0xedb88320U:0);}
    }
    CloseHandle(f);return total==FIXTURE_SIZE&&(crc^0xffffffffU)==FIXTURE_CRC;
}
void Entry(void)
{
    DWORD n;unsigned i;char*args=GetCommandLineA();int verify=0;
    for(i=0;args[i];i++)if(args[i]=='/'&&args[i+1]=='v')verify=1;
    n=GetWindowsDirectoryA(target,sizeof(target));
    if(!n||n>=sizeof(target)||!append(target,"\\system32\\drivers\\rltklant.sys"))ExitProcess(1);
    if(verify){
        if(!valid(target)){out("FAIL: installed driver differs from this fixture\r\n");ExitProcess(2);}
        out("PASS: installed RLTKLANT size and CRC32 match the test CD fixture\r\n");ExitProcess(0);
    }
    n=GetModuleFileNameA(0,source,sizeof(source));if(!n||n>=sizeof(source))ExitProcess(3);
    while(n&&source[n-1]!='\\')n--;
    source[n]=0;
    if(!append(source,FIXTURE_NAME)||!valid(source)){out("FAIL: test CD fixture mismatch\r\n");ExitProcess(4);}
    CreateDirectoryA("C:\\RLTTEST",0);
    if(!CopyFileA(source,"C:\\RLTTEST\\pcifw.sys",FALSE)||!valid("C:\\RLTTEST\\pcifw.sys")){
        out("FAIL: copied test fixture mismatch\r\n");ExitProcess(5);
    }
    if(!MoveFileExA("C:\\RLTTEST\\pcifw.sys",target,MOVEFILE_DELAY_UNTIL_REBOOT|MOVEFILE_REPLACE_EXISTING)){
        out("FAIL: cannot append delayed driver replacement\r\n");ExitProcess(6);
    }
    out("PASS: verified RLTKLANT replacement scheduled for the next normal boot\r\n");ExitProcess(0);
}
