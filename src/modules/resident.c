#include <windows.h>
#include <toolhelp.h>
#include <conio.h>
#include <stdlib.h>

#define BLOCK_MIB 16
#define BLOCK_PAGES 4096
#define MAX_BLOCKS 255

typedef struct {
    DWORD base;
    DWORD handle;
} Block;
static Block live[MAX_BLOCKS];
static char line[256];
static void say(const char *s) { while (*s) outp(0xe9, *s++); }

static int reserve(Block *b)
{
    WORD bhi, blo, hhi, hlo, ok;
    _asm {
        mov ax,0501h
        mov bx,BLOCK_MIB
        shl bx,4
        xor cx,cx
        int 31h
        sbb ax,ax
        inc ax
        mov ok,ax
        mov bhi,bx
        mov blo,cx
        mov hhi,si
        mov hlo,di
    }
    b->base=((DWORD)bhi << 16) | blo;
    b->handle=((DWORD)hhi << 16) | hlo;
    return ok;
}
static int resize(Block *b, DWORD bytes) {
    WORD hi=HIWORD(bytes),lo=LOWORD(bytes),sh=HIWORD(b->handle),sl=LOWORD(b->handle),ok;
    _asm {
        mov ax,0503h
        mov bx,hi
        mov cx,lo
        mov si,sh
        mov di,sl
        int 31h
        sbb ax,ax
        inc ax
        mov ok,ax
        mov hi,bx
        mov lo,cx
        mov sh,si
        mov sl,di
    }
    if(ok){b->base=((DWORD)hi<<16)|lo;b->handle=((DWORD)sh<<16)|sl;}
    return ok;
}
static int release(DWORD handle)
{
    WORD hhi=HIWORD(handle), hlo=LOWORD(handle), ok;
    _asm {
        mov si,hhi
        mov di,hlo
        mov ax,0502h
        int 31h
        sbb ax,ax
        inc ax
        mov ok,ax
    }
    return ok;
}
static int check_pages(WORD sel, DWORD addr, DWORD first, WORD verify)
{
    WORD hi=HIWORD(addr), lo=LOWORD(addr), ok=0;
    _asm {
        mov bx,sel
        mov cx,hi
        mov dx,lo
        mov ax,7
        int 31h
        jc done
        mov bx,sel
        mov cx,00ffh
        mov dx,0ffffh
        mov ax,8
        int 31h
        jc done
        push es
        mov ax,sel
        mov es,ax
        mov eax,first
        xor edi,edi
        mov ecx,BLOCK_PAGES
        cmp verify,0
        jne read_loop
     write_loop:
        mov dword ptr es:[edi],eax
        not eax
        mov dword ptr es:[edi+4092],eax
        not eax
        inc eax
        add edi,4096
        dec ecx
        jne write_loop
        jmp success
     read_loop:
        cmp dword ptr es:[edi],eax
        jne bad
        not eax
        cmp dword ptr es:[edi+4092],eax
        jne bad
        not eax
        inc eax
        add edi,4096
        dec ecx
        jne read_loop
     success:
        mov ok,1
     bad:
        pop es
     done:
    }
    return ok;
}
static void memory(const char *stage)
{
    MEMMANINFO m;
    m.dwSize=sizeof(m);
    if (MemManInfo(&m)) {
        wsprintf(line,"%s pages=%lu free=%lu linear=%lu linear_free=%lu swap=%lu\n",
                 (LPSTR)stage,m.dwTotalPages,m.dwFreePages,m.dwTotalLinearSpace,m.dwFreeLinearSpace,m.dwSwapFilePages);
        say(line);
    }
}
int PASCAL WinMain(HINSTANCE instance,HINSTANCE prev,LPSTR cmd,int show)
{
    unsigned i, count=0, goal, sel, failed=0;
    DWORD until;
    const char *why="";
    (void)instance; (void)prev; (void)show;
    say("W31PROBE begin\n");
    wsprintf(line,"windows=%04x flags=%08lx\n",LOWORD(GetVersion()),GetWinFlags());say(line);
    memory("before");
    goal=(unsigned)atoi(cmd)/BLOCK_MIB;
    if (!goal || goal>MAX_BLOCKS) goal=20;
    _asm {
        xor ax,ax
        mov cx,1
        int 31h
        jnc got_sel
        xor ax,ax
     got_sel:
        mov sel,ax
    }
    if (!sel) { say("no selector\nW31PROBE end\n");return 1; }
    for (i=0;i<goal;i++) {
        if (!reserve(live+i)) {failed=1;why="alloc";break;}
        ++count;
        if (!check_pages(sel,live[i].base,0x31000000ul+(DWORD)i*BLOCK_PAGES,0)) {
            failed=1;why="write";break;
        }
        if ((i&15)==15) {wsprintf(line,"written_mib=%u\n",count*BLOCK_MIB);say(line);}
    }
    if(count) {
        wsprintf(line,"resident_mib=%u first_linear=%08lx last_linear=%08lx\n",
                 count*BLOCK_MIB,live[0].base,live[count-1].base+((DWORD)BLOCK_MIB<<20)-4096);
        say(line);
        memory("resident");
        until=GetTickCount();
        while (GetTickCount()-until < 1500) {}
    }
    for(i=count;i>0;i--) {
        if (!check_pages(sel,live[i-1].base,0x31000000ul+(DWORD)(i-1)*BLOCK_PAGES,1)) {
            failed=1;why="read";break;
        }
    }
    if(!failed && count>=3) {
        for(i=0;i<count;i+=2){
            if(!release(live[i].handle)){failed=1;why="fragment free";break;}
            live[i].handle=0;
        }
        for(i=0;!failed && i<count;i+=2){
            Block fresh;
            if(!reserve(&fresh)){failed=1;why="fragment alloc";break;}
            live[i]=fresh;
            if(!check_pages(sel,fresh.base,0x51000000ul+(DWORD)i*BLOCK_PAGES,0)){failed=1;why="fragment write";}
        }
        for(i=0;!failed && i<count;i++){
            DWORD generation=(i&1)?0x31000000ul:0x51000000ul;
            if(!check_pages(sel,live[i].base,generation+(DWORD)i*BLOCK_PAGES,1)){failed=1;why="fragment read";}
        }
        if(!failed)say("fragmented reuse and untouched neighbors verified\n");
    }
    for(i=count;i>0;i--) if(live[i-1].handle && !release(live[i-1].handle)) {failed=1;why="free";}
    if(!failed) {
        Block resized;
        if(!reserve(&resized)){failed=1;why="resize alloc";}
        else {
            if(!resize(&resized,8UL<<20)||!resize(&resized,16UL<<20)||
               !check_pages(sel,resized.base,0x61000000ul,0)||
               !check_pages(sel,resized.base,0x61000000ul,1)){failed=1;why="resize";}
            if(!release(resized.handle)){failed=1;why="resize free";}
            if(!failed)say("DPMI shrink and grow verified\n");
        }
    }
    _asm {
        mov bx,sel
        mov ax,1
        int 31h
    }
    memory("after");
    wsprintf(line,"result=%s target_mib=%u checked_mib=%u reason=%s\n",
             (LPSTR)(failed?"FAIL":"PASS"),goal*BLOCK_MIB,count*BLOCK_MIB,(LPSTR)why);
    say(line);say("W31PROBE end\n");
    return failed;
}
