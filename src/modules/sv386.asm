; Windows/386 needs the original MS-DOS 3.30 layout while initializing.
; Activate its SETVER profile only after the Win16 environment has started.
bits 16
cpu 8086
org 100h
jmp install
old2f dd 0
dos_version dd 0
profile dw 0

handler:
    cmp ax,0xd74c
    jne .chain
    cmp bx,0x5653
    jne .chain
    push es
    push di
    les di,[cs:dos_version]
    mov ax,[es:di]
    cmp ax,1e03h
    je .apply
    cmp ax,[cs:profile]
    jne .done
.apply:
    mov ax,[cs:profile]
    cmp dx,0x5253
    jne .store
    mov ax,1e03h
.store:
    mov [es:di],ax
.done:
    pop di
    pop es
    iret
.chain:
    jmp far [cs:old2f]
resident_end:

install:
    cmp byte [80h],0
    je .install
    mov ax,0xd74c
    mov bx,0x5653
    mov dx,0x5253
    int 2fh
    int 20h
.install:
    mov ax,3000h
    int 21h
    cmp ax,1e03h
    jne bad
    call table_version
    jc bad
    cmp ax,2803h
    jne bad
    mov [profile],ax
    ; The version word and its adjacent constant DOS-country data identify
    ; this exact MS-DOS 3.30 build. ES from AH=52h selects DOS system data.
    mov ah,52h
    int 21h
    xor di,di
.find:
    cmp word [es:di],1e03h
    jne .next
    cmp word [es:di+2],0xa6c8
    jne .next
    cmp word [es:di+4],0xa5c8
    jne .next
    cmp word [es:di+6],0xa5c8
    jne .next
    mov [dos_version],di
    mov [dos_version+2],es
    mov ax,352fh
    int 21h
    mov [old2f],bx
    mov [old2f+2],es
    mov dx,handler
    mov ax,252fh
    int 21h
    mov dx,(resident_end-$$+100h+15)/16
    mov ax,3100h
    int 21h
.next:
    inc di
    cmp di,0xfff0
    jb .find
bad:
    mov ax,4c01h
    int 21h
%include "svtable.inc"
