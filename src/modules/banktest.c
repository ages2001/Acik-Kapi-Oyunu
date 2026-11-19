#include <windows.h>
#include <conio.h>
#include <dos.h>
#include <string.h>
struct RM { DWORD edi,esi,ebp,reserved,ebx,edx,ecx,eax; WORD flags,es,ds,fs,gs,ip,cs,sp,ss; };
static struct RM regs;
static WORD segment,offset;
static DWORD size_k;
static void logtext(const char *s){while(*s)outp(0xe9,*s++);}
static void number(DWORD n){char b[12];int i=0;do{b[i++]=(char)('0'+n%10);n/=10;}while(n);while(i)outp(0xe9,b[--i]);}
static int invoke(int pm,WORD fn,DWORD position,WORD count){
    DWORD p=position; WORD r=0,sg=0,off=0,high=0;
    if(pm){
        memset(&regs,0,sizeof(regs));regs.eax=0xd74d;regs.ebx=0x4258;regs.ecx=fn;regs.edx=LOWORD(position);regs.ebp=HIWORD(position);regs.esi=count;
        _asm {
            push es
            push ds
            pop es
            mov di,offset regs
            mov bx,002fh
            xor cx,cx
            mov ax,0300h
            int 31h
            pop es
            jc bad
            mov r,1
        bad:
        }
        if(!r)return 0;
        r=(WORD)regs.eax;sg=(WORD)regs.esi;off=(WORD)regs.edi;p=MAKELONG((WORD)regs.edx,(WORD)regs.ecx);
    }else{
        _asm {
            mov si,count
            mov edx,p
            mov ax,0d74dh
            mov bx,04258h
            mov cx,fn
            push bp
            mov ebp,edx
            shr ebp,16
            int 2fh
            pop bp
            mov high,cx
            mov r,ax
            mov sg,si
            mov off,di
            mov p,edx
        }
    }
    if(!pm)p=MAKELONG(LOWORD(p),high);
    if(r!=0x5842)return 0;
    if(fn==0){segment=sg;offset=off;size_k=p;}
    return 1;
}
int PASCAL WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show){
    int pm=0,pass,full=(*cmd=='B');
    WORD sel,i;DWORD k,last; BYTE __far *buffer;
    (void)h;(void)prev;(void)show;
#ifdef WIN3
    pm=(GetWinFlags()&WF_PMODE)!=0;
#endif
    logtext("W31PROBE begin\n");
    if(!invoke(pm,0,0,0))goto fail;
    logtext("Shared XMS bank KiB=");number(size_k);logtext("\n");
    if(size_k<64)goto fail;
    sel=segment;
    if(pm){
        _asm {
            mov bx,sel
            mov ax,0002h
            int 31h
            jc noseg
            mov sel,ax
            jmp done
        noseg:
            mov sel,0
        done:
        }
    }
    if(!sel){logtext("no real-mode segment selector\n");goto fail;}
    logtext("transfer buffer selector or segment=");number(sel);logtext(" offset=");number(offset);logtext("\n");
    buffer=(BYTE __far *)MK_FP(sel,offset);last=size_k-32;
    for(pass=0;pass<2;pass++){
        for(k=0;;){
            for(i=0;i<8;i++){
                DWORD tag=0x53000000ul+(k>>2)+i;
                *(DWORD __far *)(buffer+(unsigned)i*4096)=pass?0:tag;
                *(DWORD __far *)(buffer+(unsigned)i*4096+4092)=pass?0:~tag;
            }
            if(!invoke(pm,pass?1:2,k<<10,32768)){logtext("move failed KiB=");number(k);logtext(" pass=");number(pass);logtext("\n");goto fail;}
            if(pass)for(i=0;i<8;i++){
                DWORD tag=0x53000000ul+(k>>2)+i;
                if(*(DWORD __far *)(buffer+(unsigned)i*4096)!=tag||
                   *(DWORD __far *)(buffer+(unsigned)i*4096+4092)!=~tag){logtext("verify failed KiB=");number(k);logtext(" actual=");number(*(DWORD __far *)(buffer+(unsigned)i*4096));logtext("\n");goto fail;}
            }
            if(k==last)break;
            if(full)k+=32;else if(k==0)k=(last>>1)&~31ul;else k=last;
        }
    }
    logtext(full?"result=PASS all bank pages\n":"result=PASS low, middle and high bank addresses\n");
    logtext("W31PROBE end\n");return 0;
fail:
    logtext("result=FAIL bank transfer\nW31PROBE end\n");return 1;
}
