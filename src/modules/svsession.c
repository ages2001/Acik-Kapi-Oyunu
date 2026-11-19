#include <windows.h>
#include <conio.h>
/* Windows/386 has finished initializing its DOS-private structures before
   WIN.INI's load= application runs. Read the configured profile at that point. */
struct LoadArgs { WORD env; void FAR *args; void FAR *show; DWORD reserved; };
static char child[80],tail[128];
static WORD display[2]={2,SW_SHOWNORMAL};
static struct LoadArgs params;
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR command,int show) {
    unsigned profile=0;
    const char *s;unsigned i;
    (void)h;(void)prev;(void)command;(void)show;
    _asm {
        mov ax,0d74ch
        mov bx,5653h
        xor dx,dx
        int 2fh
        mov profile,ax
    }
    if (profile!=0x2803) {
        MessageBox(0,"SETVER 3.40 profile is unavailable","Windows/386",MB_OK);
        return 1;
    }
    for(s="Windows/386 activated SETVER 3.40\n";*s;s++)outp(0xe9,*s);
    while (*command==' ') command++;
    if (*command) {
        for(i=0;*command && *command!=' ' && i+1<sizeof(child);i++) child[i]=*command++;
        child[i]=0;while (*command==' ') command++;
        for(i=0;*command && i+2<sizeof(tail);i++)tail[i+1]=*command++;
        tail[0]=(char)i;tail[i+1]=13;params.args=tail;params.show=display;
        return (unsigned)LoadModule(child,&params)<32;
    }
    return 0;
}
