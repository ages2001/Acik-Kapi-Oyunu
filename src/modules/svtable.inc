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
