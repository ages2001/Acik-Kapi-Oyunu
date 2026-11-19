#include <windows.h>
#include <conio.h>
/* Exercise the public memory source while the shell About window stays open. */
static DWORD xms_api,linear,handle;
static WORD xhandle,source;
static void say(const char *s){while(*s)outp(0xe9,*s++);}
static int change(int release){WORD ok=0;
 if(source==1){
  _asm {mov ax,0d74dh
   mov bx,04258h
   mov cx,5
   add cx,release
   mov edx,32
   mov esi,512
   int 2fh
   cmp ax,05842h
   jne bd
   mov ok,1
  bd:}
 }else if(source==0){
  _asm {mov ax,4310h
   int 2fh
   mov word ptr xms_api,bx
   mov word ptr xms_api+2,es
   cmp release,0
   jne xr
   mov edx,16384
   mov ah,89h
   call dword ptr xms_api
   mov xhandle,dx
   jmp xd
  xr:mov dx,xhandle
   mov ah,0ah
   call dword ptr xms_api
  xd:mov ok,ax}
 }else{
  if(!release){
   _asm {mov ax,0501h
    mov bx,0100h
    xor cx,cx
    int 31h
    jc noalloc
    mov word ptr linear,cx
    mov word ptr linear+2,bx
    mov word ptr handle,di
    mov word ptr handle+2,si
    mov si,0100h
    xor di,di
    mov ax,0600h
    int 31h
    jc noalloc
    mov ok,1
   noalloc:}
  }else{
   _asm {mov bx,word ptr linear+2
    mov cx,word ptr linear
    mov si,0100h
    xor di,di
    mov ax,0601h
    int 31h
    mov si,word ptr handle+2
    mov di,word ptr handle
    mov ax,0502h
    int 31h
    jc nofree
    mov ok,1
   nofree:}
  }}
 return ok;}
int PASCAL WinMain(HINSTANCE ins,HINSTANCE prev,LPSTR args,int show){MSG m;UINT timer;int ticks=0;WORD status=0,bank=0;
 (void)ins;(void)prev;(void)args;(void)show;
 _asm {mov ax,0d74fh
  mov bx,05855h
  int 2fh
  mov status,ax
  mov bank,bx}
 source=status==0x5842?(bank?1:0):2;
 timer=SetTimer(0,0,500,0);if(!timer)return 1;
 while(GetMessage(&m,0,0,0)){
  if(m.message==WM_TIMER){ticks++;if(ticks==24)say(change(0)?"PROBE allocated 16384 KiB\n":"PROBE allocation failed\n");
   if(ticks==36){say(change(1)?"PROBE released 16384 KiB\n":"PROBE release failed\n");break;}}
  else {TranslateMessage(&m);DispatchMessage(&m);}}
 KillTimer(0,timer);return 0;}
