bits 16
cpu 286
org 0x7c20
%define ROOT 0xa5e
%define STRIDE 28
%define FIRST 6
%define MASK 2
; GDI stores a 4-byte movable handle followed here by the same 24-byte
; brush data record that its CreateBrushIndirect path normally requests.
allocate:
    cmp si,0x4f48
    jne .original
    cmp bx,24
    jne .original
    push es
    push di
    les di,[bp+6]
    cmp word [es:di],0
    pop di
    pop es
    jne .original
    push si
    push di
    push es
    mov bx,[ROOT]
.scan:
    test bx,bx
    jz .new
    cmp word [bx+MASK],0xffff
    jne .use
    mov bx,[bx]
    jmp .scan
.new:
    push byte 0x40
    push word (FIRST+16*STRIDE)
    db 0x9a
    dw 0xffff,0xa110
    test ax,ax
    jz .single
    mov bx,ax
    mov ax,[ROOT]
    mov [bx],ax
    mov [ROOT],bx
.use:
    mov cx,1
    lea di,[bx+FIRST]
.slot:
    test [bx+MASK],cx
    jz .take
    shl cx,1
    add di,STRIDE
    jmp .slot
.take:
    or [bx+MASK],cx
    mov dx,di
    push ds
    pop es
    mov cx,STRIDE/2
    xor ax,ax
    cld
    rep stosw
    mov bx,dx
    lea ax,[bx+4]
    mov [bx],ax
    mov ax,bx
    mov cx,ax
    pop es
    pop di
    pop si
    ret
.single:
    pop es
    pop di
    pop si
    mov bx,24
.original:
    push bx
    call 0x1f43
    ret

free:
    ; DX is the handle, SI is the already saved next realization.
    push bx
    push si
    push cx
    mov bx,[ROOT]
    xor si,si
.scan:
    or bx,bx
    jz .other
    mov ax,dx
    sub ax,bx
    sub ax,FIRST
    cmp ax,(16*STRIDE)
    jb .found
    mov si,bx
    mov bx,[bx]
    jmp .scan
.found:
    xor dx,dx
    mov cx,STRIDE
    div cx
    mov cx,ax
    mov ax,1
    shl ax,cl
    not ax
    and [bx+MASK],ax
    jnz .done
    mov ax,[bx]
    or si,si
    jz .head
    mov [si],ax
    jmp .release
.head:
    mov [ROOT],ax
.release:
    push bx
    jmp .call
.other:
    push dx
.call:
    db 0x9a
    dw 0xffff,0xf3ee
.done:
    pop cx
    pop si
    pop bx
    ret
dw allocate,free
