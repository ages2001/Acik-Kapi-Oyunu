bits 16
cpu 286
org 0x14a0
%define ROOT 0x0ab8
%define COUNT 16
%define ITEM 18
%macro kernel_call 1
    db 0x9a
    dw 0xffff,%1
%endmacro
save:
; Input DS is the private menu segment. The live window-manager menu header
; remains exactly 18 bytes and its handle is still its near address.
allocate:
    push bx
    push cx
    push dx
    push si
    push di
    push es
    mov ax,0xf1fa                  ; DGROUP selector, resolved by the NE loader
    mov es,ax
    mov bx,[es:ROOT]
.scan:
    or bx,bx
    jz .new
    cmp word [bx+2],0xffff
    jne .available
    mov bx,[bx]
    jmp .scan
.new:
    push es
    push byte 0x40
    push word (4+COUNT*ITEM)
    kernel_call 0xa110
    pop es
    or ax,ax
    jz .single
    mov bx,ax
    mov ax,[es:ROOT]
    mov [bx],ax
    mov [es:ROOT],bx
.available:
    mov cx,1
    lea di,[bx+4]
.slot:
    test [bx+2],cx
    jz .take
    shl cx,1
    add di,ITEM
    jmp .slot
.take:
    or [bx+2],cx
    push ds
    pop es
    mov dx,di
    xor ax,ax
    mov cx,ITEM/2
    cld
    rep stosw
    mov ax,dx
    jmp .done
.single:
    push byte 0x40
    push byte ITEM
    kernel_call 0xa110
.done:
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    ret

free:
    push bx
    push cx
    push dx
    push si
    push es
    mov ax,0xf1fa
    mov es,ax
    mov bx,[es:ROOT]
    xor si,si
.scan:
    or bx,bx
    jz .single
    mov ax,di
    sub ax,bx
    sub ax,4
    cmp ax,COUNT*ITEM
    jb .found
    mov si,bx
    mov bx,[bx]
    jmp .scan
.found:
    xor dx,dx
    mov cx,ITEM
    div cx
    mov cx,ax
    mov ax,1
    shl ax,cl
    not ax
    and [bx+2],ax
    jnz .done
    mov ax,[bx]
    or si,si
    jz .head
    mov [si],ax
    jmp .empty
.head:
    mov [es:ROOT],ax
.empty:
    push bx
    jmp .release
.single:
    push di
.release:
    kernel_call 0xf3ee
.done:
    pop es
    pop si
    pop dx
    pop cx
    pop bx
    ret
dw allocate,free
