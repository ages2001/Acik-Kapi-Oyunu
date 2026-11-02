/* Guest fixture only: defer a locked test driver replacement until reboot.
 * MoveFileEx appends to the OS-managed list; never replace that registry value.
 * The fixture was transferred and checked separately before invoking this tool.
 */
#define _WIN32_WINNT 0x0500
#include <windows.h>
static void out(const char *s) {
    DWORD n=0,w;while(s[n])n++;WriteFile(GetStdHandle(STD_OUTPUT_HANDLE),s,n,&w,0);
}
void Entry(void) {
    HANDLE f;DWORD size;
    const char *src="C:\\RLTTEST\\ustall32.sys";
    f=CreateFileA(src,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,0,0);
    if(f==INVALID_HANDLE_VALUE){out("FAIL: staged USBNT fixture missing\r\n");ExitProcess(1);}
    size=GetFileSize(f,0);CloseHandle(f);
    if(size!=480768){out("FAIL: unexpected staged USBNT size\r\n");ExitProcess(2);}
    if(!MoveFileExA(src,"C:\\WINNT\\system32\\drivers\\usbnt.sys",
                   MOVEFILE_DELAY_UNTIL_REBOOT|MOVEFILE_REPLACE_EXISTING)){
        out("FAIL: could not schedule USBNT replacement\r\n");ExitProcess(3);
    }
    out("PASS: USBNT replacement scheduled for the next normal boot\r\n");
    ExitProcess(0);
}
