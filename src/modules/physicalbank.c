#include <windows.h>
#include <conio.h>
#include <string.h>
struct RM { DWORD di,si,bp,res,bx,dx,cx,ax; WORD flags,es,ds,fs,gs,ip,cs,sp,ss; };
static struct RM mm;
static void txt(const char*s){while(*s) outp(0xe9,*s++);}
static void num(DWORD n){char b[11];int i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)outp(0xe9,b[--i]);}
static DWORD query(int id){
    memset(&mm,0,sizeof(mm));mm.ax=0xd74d;mm.bx=0x4258;mm.cx=id;
    _asm {
        push es
        push ds
        pop es
        mov di,offset mm
        mov ax,0300h
        mov bx,002fh
        xor cx,cx
        int 31h
        pop es
    }
    return (WORD)mm.ax==0x5842?MAKELONG((WORD)mm.dx,(WORD)mm.cx):0;
}
static DWORD map_page(DWORD phys){
    WORD hi=HIWORD(phys),lo=LOWORD(phys),ok=0;
    _asm {
        mov ax,0800h
        mov bx,hi
        mov cx,lo
        xor si,si
        mov di,4096
        int 31h
        jc failure
        mov hi,bx
        mov lo,cx
        mov ok,1
    failure:
    }
    return ok?MAKELONG(lo,hi):0;
}
int PASCAL WinMain(HINSTANCE a,HINSTANCE b,LPSTR c,int d){
    DWORD size,phys,lin[3],tag;WORD sel=0,hi,lo;int i,p;
    (void)a;(void)b;(void)c;(void)d;txt("W31PROBE begin\n");
    size=query(0);phys=query(3);txt("bank KiB=");num(size);txt(" physical base=");num(phys);txt("\n");
    if(!size||!phys)goto fail;
    lin[0]=map_page(phys);lin[1]=map_page(phys+((size/2)&~3UL)*1024);lin[2]=map_page(phys+(size-4)*1024);
    for(i=0;i<3;i++){txt("linear=");num(lin[i]);txt("\n");if(!lin[i])goto fail;}
    _asm {
        xor ax,ax
        mov cx,1
        int 31h
        jc errsel
        mov sel,ax
    errsel:
    }
    if(!sel)goto fail;
    for(p=0;p<2;p++)for(i=0;i<3;i++){
        DWORD __far *address=(DWORD __far *)MAKELONG(0,sel);
        hi=HIWORD(lin[i]);lo=LOWORD(lin[i]);
        _asm {
            mov bx,sel
            mov ax,7
            mov cx,hi
            mov dx,lo
            int 31h
            mov bx,sel
            mov ax,8
            xor cx,cx
            mov dx,4095
            int 31h
        }
        tag=0x74003100UL+i;if(!p){address[0]=tag;address[1023]=~tag;}
        else if(address[0]!=tag||address[1023]!=~tag)goto fail;
    }
    hi=HIWORD(lin[2]);lo=LOWORD(lin[2]);
    _asm {
        mov bx,hi
        mov cx,lo
        mov ax,0801h
        int 31h
        sbb dx,dx
        mov lo,dx
    }
    txt(lo?"DPMI 0801h unavailable\n":"DPMI 0801h available\n");
    txt("result=PASS three mapped bank pages\nW31PROBE end\n");return 0;
fail: txt("result=FAIL physical bank mapping\nW31PROBE end\n");return 1;
}
