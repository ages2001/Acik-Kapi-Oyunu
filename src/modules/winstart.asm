; Propagate the actual SETVER entry for WINSTART.COM through a Windows 1/2
; session. Win16 creates task PSPs internally and DOS never EXECs their NE files.
cpu 8086
bits 16
org 100h
jmp start
old21 dd 0
reported dw 0
pars dw 0,tail,0,5ch,0,6ch,0
file times 64 db 0
tail times 128 db 0
message db 'WINSTART needs its MS-DOS SETVER entry (3.30 or 3.40).',13,10,'$'
usage db 'Usage: WINSTART WIN.COM [application and arguments]',13,10,'$'
execerr db 'DOS could not start Windows.',13,10,'$'
hook:
    cmp ah,30h
    jne .chain
%ifdef LOADER386
    ; Windows/386 uses INT 21h at four fixed sites while choosing DOS-private
    ; data structures. On PC DOS 3.30 those queries must see its true layout.
    push bp
    mov bp,sp
    cmp word [bp+2],0x240b
    je .loader
    cmp word [bp+2],0x2aa2
    je .loader
    cmp word [bp+2],0x3c89
    je .loader
    cmp word [bp+2],0x53c5
    je .loader
    pop bp
%endif
    pushf
    call far [cs:old21]
    mov ax,[cs:reported]
    iret
%ifdef LOADER386
.loader:
    pop bp
%endif
.chain:
    jmp far [cs:old21]
start:
    mov sp,stackend
    mov bx,(stackend-$$+100h+15)/16
    mov ah,4ah
    int 21h
    jc .execfail
    mov ax,3000h
    int 21h
%ifdef TABLE_FILE
    call table_version
    jc .misconfigured
%endif
    cmp ax,1e03h
    je .valid
    cmp ax,2803h
    jne .misconfigured
.valid:
    mov [reported],ax
    mov ax,ds
    mov [pars+4],ax
    mov [pars+8],ax
    mov [pars+12],ax
    mov si,81h
    xor cx,cx
    mov cl,[80h]
.space:
    jcxz .usage
    cmp byte [si],' '
    jne .name
    inc si
    dec cx
    jmp .space
.name:
    mov di,file
.copyname:
    jcxz .runtail
    cmp byte [si],' '
    je .runtail
    cmp di,file+63
    jae .usage
    movsb
    dec cx
    jmp .copyname
.runtail:
    mov [tail],cl
    mov di,tail+1
    rep movsb
    mov byte [di],13
    mov ax,3521h
    int 21h
    mov [old21],bx
    mov [old21+2],es
    mov dx,hook
    mov ax,2521h
    int 21h
    push ds
    pop es
    mov bx,pars
    mov dx,file
    mov ax,4b00h
    int 21h
    pushf
    push cs
    pop ds
    lds dx,[old21]
    mov ax,2521h
    int 21h
    push cs
    pop ds
    popf
    jc .execfail
    mov ah,4dh
    int 21h
    mov ah,4ch
    int 21h
.usage:
    mov dx,usage
    jmp .error
.misconfigured:
    mov dx,message
    jmp .error
.execfail:
    mov dx,execerr
.error:
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h
%ifdef TABLE_FILE
; DOS 3 does not have the later kernel's SETVER device hook. Read the actual
; configuration written by Microsoft's SETVER utility on the DOS 6 maintenance disk.
table_version:
    mov ax,3d00h
    mov dx,pathname
    int 21h
    jc .bad
    mov bx,ax
    mov cx,32
    mov dx,scratch
    mov ah,3fh
    int 21h
    jc .closebad
    cmp word [scratch],5a4dh
    jne .closebad
    mov ax,[scratch+8]
    mov cl,4
    shl ax,cl
    mov [modbase],ax
    mov dx,ax
    xor cx,cx
    mov ax,4200h
    int 21h
    jc .closebad
    mov dx,scratch
    mov cx,28
    mov ah,3fh
    int 21h
    jc .closebad
    cmp word [scratch+10],'SE'
    jne .closebad
    cmp word [scratch+12],'TV'
    jne .closebad
    cmp word [scratch+14],'ER'
    jne .closebad
    mov dx,[scratch+20]
    add dx,[modbase]
    mov cx,[scratch+22]
    adc cx,0
    mov ax,4200h
    int 21h
    jc .closebad
    mov dx,scratch
    mov cx,2048
    mov ah,3fh
    int 21h
    jc .closebad
    mov bp,ax
    mov ah,3eh
    int 21h
    mov si,scratch
.next:
    cmp bp,3
    jb .bad
    xor ax,ax
    lodsb
    test ax,ax
    jz .bad
    sub bp,3
    sub bp,ax
    jc .bad
    cmp al,12
    jne .skip
    mov di,entryname
    mov cx,12
    push si
    repe cmpsb
    pop si
    jne .skip
    mov ax,[si+12]
    clc
    ret
.skip:
    add si,ax
    add si,2
    jmp .next
.closebad:
    mov ah,3eh
    int 21h
.bad:
    stc
    ret
pathname db 'WRMTOOLS',92,'SETVER.EXE',0
entryname db 'WINSTART.COM'
modbase dw 0
scratch times 2048 db 0
%endif
times 512 db 0
stackend:
