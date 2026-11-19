bits 16
cpu 386
org ORIGIN
; The 1.01 bootstrap writes a word across the old segment's odd end.
; Begin executable entries after the loader's paragraph workspace.
times 32 db 0
%macro api 2
api_%1:
    push word %2
    push word %1
    jmp near resolve
%endmacro
%include "ordinals.inc"
resolve:
    push bp
    mov bp,sp
    push ax
    push bx
    mov ax,[cs:provider]
    test ax,ax
    jz .native
    cmp ax,[ss:bp+8]
    je .native
    mov bx,[ss:bp+2]
    shl bx,1
    mov bx,[cs:vectors+bx]
    test bx,bx
    jz .native
    mov [ss:bp+2],bx
    mov [ss:bp+4],ax
    jmp short .done
.native:
    mov ax,[ss:bp+4]
    mov [ss:bp+2],ax
    mov [ss:bp+4],cs
.done:
    pop bx
    pop ax
    pop bp
    retf

; Windows 1.01 exports this session initialiser as KERNEL.101.
; Only the installed resident WRMPAGE module can enable its own entry points.
activate:
    push bp
    mov bp,sp
    pusha
    push ds
    push es
    push cs
    push word module_name
    push cs
    call near MODULE_OFF
    or ax,ax
    jz .done
    mov di,ax
    mov si,15
.next:
    push si
    push di
    push di
    push word 0
    push si
    push cs
    call near PROC_OFF
    pop di
    pop si
    cmp dx,[ss:bp+4]
    jne .done
    mov bx,si
    shl bx,1
    mov [cs:vectors+bx],ax
    inc si
    cmp si,25
    jne .next
    push dx
    push cs
    push word kernel_name
    push cs
    call near MODULE_OFF
    pop dx
    test ax,ax
    jz .done
    mov es,ax
    cmp word [es:0],0x454e
    jne .done
    mov di,[es:4]
    cmp word [es:di],0x0164
    jne .done
    add di,3+14*3
    mov bx,di
    mov si,changes
    mov cx,10
.check:
    mov ax,[cs:si]
    cmp [es:di],ax
    je .matched
    mov ax,[cs:si+2]
    cmp [es:di],ax
    jne .done
.matched:
    add di,3
    add si,4
    loop .check
    mov [cs:provider],dx
    mov di,bx
    mov si,changes+2
    mov cx,10
.apply:
    mov ax,[cs:si]
    mov [es:di],ax
    add di,3
    add si,4
    loop .apply
.done:
    pop es
    pop ds
    popa
    pop bp
    mov ax,[cs:provider]
    retf
deactivate:
    push bp
    mov bp,sp
    push ax
    mov ax,[ss:bp+4]
    cmp ax,[cs:provider]
    jne .return
    mov word [cs:provider],0
.return:
    pop ax
    pop bp
    retf
kernel_name db 'KERNEL',0
module_name db 'WRMPAGE',0
provider dw 0
vectors equ $-15*2
times 10 dw 0
changes:
%unmacro api 2
%macro api 2
    dw %2,api_%1
%endmacro
%include "ordinals.inc"
dw activate,deactivate
