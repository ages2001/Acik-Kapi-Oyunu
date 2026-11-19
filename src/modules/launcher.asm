; Interpose on the normal DOS Windows command and pass its original arguments.
bits 16
cpu 386
org 100h
start:
    mov sp,stacktop
    mov bx,(stacktop-$$+100h+15)/16
    mov ah,4ah
    int 21h
    jc error
    mov ax,ds
    mov [args+4],ax
    mov [args+8],ax
    mov [args+12],ax
    mov es,[2ch]
    xor di,di
.seek:
    cmp word [es:di],0
    je .found
    inc di
    jnz .seek
    jmp error
.found:
    add di,4
    mov si,dir
    mov bx,si
.name:
    mov al,[es:di]
    inc di
    test al,al
    jz .cd
    cmp si,dir+125
    jae error
    mov [si],al
    cmp al,92
    jne .next
    mov bx,si
.next:
    inc si
    jmp .name
.cd:
    mov byte [bx],0
    mov ah,19h
    int 21h
    mov [saveddrive],al
    mov dl,[dir]
    and dl,0dfh
    sub dl,'A'
    mov [targetdrive],dl
    inc dl
    mov si,oldcwd+3
    mov ah,47h
    int 21h
    jc error
    mov dl,[targetdrive]
    mov al,dl
    add al,'A'
    mov [oldcwd],al
    mov ah,0eh
    int 21h
    mov dx,dir
    cmp bx,dir+2
    jne .change
    mov word [bx],92
.change:
    mov ah,3bh
    int 21h
    jc error
    push ds
    pop es
%if FAMILY = 30 || FAMILY = 31
    mov cl,[80h]
    xor ch,ch
    mov si,81h
.scan:
    jcxz .decide
    lodsb
    dec cx
    cmp al,'/'
    jne .scan
    mov al,[si]
    and al,0dfh
    cmp al,'R'
    jne .std
    mov byte [mode],1
.std:
    cmp al,'S'
    je .setstd
    cmp byte [si],'2'
    jne .scan
.setstd:
    mov byte [mode],2
    jmp .scan
.decide:
%endif
%if FAMILY = 30
    cmp byte [mode],1
    je .cap
    mov dx,bank
    call helper
.cap:
%elif FAMILY = 31
    cmp byte [mode],2
    jne .launch
    mov dx,bank
    call helper
%elif FAMILY = 386
    mov dx,bank
    call helper
    mov dx,sv
    call helper
%endif
    mov dx,ramcap
    call helper
.launch:
%if FAMILY = 12
    xor cx,cx
    mov cl,[80h]
    cmp cx,113
    ja error
    mov bx,cx
    add bl,12
    mov [fulltail],bl
    mov si,81h
    mov di,fulltail+13
    rep movsb
    mov byte [di],13
    mov dx,versioner
    mov word [args+2],fulltail
%else
    mov dx,core
    mov word [args+2],80h
%endif
    call execute
    jc error
    mov ah,4dh
    int 21h
    mov [rc],al
%if FAMILY = 386
    mov dx,sv
    mov word [args+2],restore
    call execute
%endif
    mov dx,oldcwd
    mov ah,3bh
    int 21h
    mov dl,[saveddrive]
    mov ah,0eh
    int 21h
    mov al,[rc]
    mov ah,4ch
    int 21h
helper:
    mov word [args+2],empty
execute:
    push ds
    pop es
    mov bx,args
    mov ax,4b00h
    int 21h
    pushf
    push cs
    pop ds
    push ds
    pop es
    popf
    ret
error:
    mov dx,msg
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h
args dw 0,empty,0,5ch,0,6ch,0
empty db 0,13
restore db 9,' /RESTORE',13
oldcwd db 'A:',92
    times 127 db 0
dir times 128 db 0
saveddrive db 0
targetdrive db 0
mode db 0
rc db 0
%if FAMILY = 386
core db 'WRMCORE.EXE',0
%else
core db 'WRMCORE.COM',0
%endif
ramcap db 'WRMTOOLS',92,'RAMCAP.COM',0
bank db 'WRMTOOLS',92,'BANKX.COM',0
sv db 'WRMTOOLS',92,'SV386.COM',0
versioner db 'WRMTOOLS',92,'WINSTART.COM',0
fulltail db 0,' WRMCORE.COM '
    times 130 db 0
msg db 'WINRAM could not start Windows.',13,10,'$'
    times 512 db 0
stacktop:
