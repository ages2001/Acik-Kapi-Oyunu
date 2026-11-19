bits 16
cpu 386
org ORIGIN
; The saved return frame stays intact while resolving the matching memory API.
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
    pusha
    push ds
    push es
    push cs
    push word module_name
    db 09ah
lookup_module: dw 0ffffh,0
    test ax,ax
    jz native_path
    push ax
    push word 0
    push word [ss:bp+2]
    db 09ah
lookup_proc: dw 0ffffh,0
    test dx,dx
    jz native_path
    cmp dx,[ss:bp+8]
    je native_path
    mov [ss:bp+2],ax
    mov [ss:bp+4],dx
    jmp short dispatch_done
native_path:
    mov ax,[ss:bp+4]
    mov [ss:bp+2],ax
    mov [ss:bp+4],cs
dispatch_done:
    pop es
    pop ds
    popa
    pop bp
    retf
module_name db 'WRMPAGE',0
trailer: dw lookup_module,lookup_proc
