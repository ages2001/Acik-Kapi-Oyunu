#include <windows.h>
#include <conio.h>
#include <dos.h>

/* A Windows 1.x-compatible entry point. Do not import Win3-only APIs on old loaders. */
static char msg[512];
static char *p;
extern unsigned __far __pascal xmscheck(void);
static void append(const char *s) { while (*s) *p++=*s++; *p=0; }
static void decimal(DWORD v) {
    char d[11];
    unsigned n=0;
    do { d[n++]='0'+(unsigned)(v%10); v/=10; } while (v);
    while(n) *p++=d[--n];
    *p=0;
}
static void emit(const char *s) { while(*s) outp(0xe9,*s++); }

int PASCAL WinMain(HINSTANCE ins,HINSTANCE prev,LPSTR cmd,int show) {
    DWORD v,free_space;
#if defined(WIN3)
    DWORD flags;
#endif
    unsigned xseg=0,xoff=0,xver=0; DWORD xfree=0;
    void (__far *driver)(void);
    (void)ins;(void)prev;(void)show;
#if defined(SESSION386)
    emit("Starting Windows/386 SETVER session\n");
    _asm {
        mov ax,0d74ch
        mov bx,5653h
        int 2fh
        mov xoff,ax
    }
    if (xoff!=0x2803) emit("SETVER table activation failed\n");
    xoff=0;
#endif
    { unsigned ver=0;
      _asm {
          mov ax,3000h
          int 21h
          mov ver,ax
      }
      p=msg;append("DOS reported by INT 21h/30h = ");decimal(ver&255);append(".");
      if((ver>>8)<10)append("0");decimal(ver>>8);append("\r\n");
    }
    emit("W31PROBE begin\n");
#if defined(WIN3)
    v=GetVersion();
    append("Windows ");decimal(LOBYTE(LOWORD(v)));append(".");decimal(HIBYTE(LOWORD(v)));append("\r\n");
    free_space=GetFreeSpace(0);
    append("GetFreeSpace bytes = ");decimal(free_space);append("\r\n");
    flags=GetWinFlags();append("GetWinFlags = ");decimal(flags);append("\r\n");
    if ((flags & WF_PMODE) == 0) {
#endif
    emit("query XMS\n");
    _asm {
        mov ax,4300h
        int 2fh
        cmp al,80h
        jne no_driver
        mov ax,4310h
        int 2fh
        mov xseg,es
        mov xoff,bx
    no_driver:
    }
    if(xseg) {
        driver=(void (__far *)(void))MK_FP(xseg,xoff);
        _asm {
            xor ah,ah
            call dword ptr driver
            mov xver,ax
        }
        append("XMS version = ");decimal(xver>>8);append(".");decimal(xver&255);append("\r\n");
        if(xver>=0x300) {
            _asm {
                mov ah,88h
                call dword ptr driver
                mov xfree,edx
            }
            append("XMS free KiB = ");decimal(xfree);append("\r\n");
        }
    }
#if defined(WIN3)
    }
#endif
    emit(msg);
    if (*cmd=='T' && xver>=0x300) return xmscheck();
    emit("W31PROBE end\n");
    if (*cmd!='!') MessageBox(0,msg,"Memory",MB_OK);
    return 0;
}
