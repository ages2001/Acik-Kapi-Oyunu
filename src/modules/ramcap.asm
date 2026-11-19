; Cache the boot-time XMS or reserved bank capacity for 386+ Win16 About boxes.
bits 16
cpu 386
org 100h
jmp install
old2f dd 0
standard times 22 db 0
enhanced times 22 db 0
realmode times 22 db 0
handler:
    cmp ax,0xd74f
    jne .chain
    cmp bx,0x5856
    je .live
    cmp bx,0x5855
    je .capacity
    cmp bx,0x5854
    jne .chain
    mov si,realmode
    test dl,1
    jz .copy
    mov si,standard
    test dl,0x10
    jnz .copy
    mov si,enhanced
.copy:
    mov ebx,[cs:si]
    mov ecx,[cs:si+4]
    mov edx,[cs:si+8]
    mov edi,[cs:si+16]
    mov esi,[cs:si+12]
    mov ax,0x5842
    iret
.live:
    push ds
    push es
    push si
    push di
    push bp
    cmp byte [cs:suffix],'b'
    jne .live_xms
    mov ax,0xd74d
    mov bx,0x4258
    mov cx,4
    int 2fh
    cmp ax,0x5842
    jne .unavailable
    mov bx,1
    jmp short .return
.live_xms:
    mov ah,88h
    call far [cs:xms_api]
    test bl,bl
    jnz .unavailable
    mov ax,0x5842
    xor bx,bx
    jmp short .return
.unavailable:
    xor ax,ax
.return:
    pop bp
    pop di
    pop si
    pop es
    pop ds
    iret
.capacity:
    mov edx,[cs:number]
    xor bx,bx
    cmp byte [cs:suffix],'b'
    jne .ok
    inc bx
.ok:
    mov ax,0x5842
    iret
.chain:
    jmp far [cs:old2f]
xms_api dd 0
number dd 0
suffix db 'XMS',0
last:
fmt:
    mov eax,[number]
    mov ebx,10
    xor cx,cx
.div:
    xor edx,edx
    div ebx
    push dx
    inc cx
    test eax,eax
    jnz .div
.dig:
    pop ax
    add al,'0'
    stosb
    loop .dig
    mov eax,' KiB'
    stosd
    mov al,' '
    stosb
    mov eax,[suffix]
    stosd
    ret
install:
    mov ax,0xd74d
    mov bx,0x4258
    xor cx,cx
    int 2fh
    cmp ax,0x5842
    jne .xms
    mov [number],edx
    mov dword [suffix],'bank'
    jmp .format
.xms:
    mov ax,4300h
    int 2fh
    cmp al,80h
    jne .err
    mov ax,4310h
    int 2fh
    mov [xms_api],bx
    mov [xms_api+2],es
    push es
    push bx
    mov si,sp
    xor ah,ah
    call far [ss:si]
    cmp ax,0300h
    jb .err
    mov ah,88h
    call far [ss:si]
    add sp,4
    test bl,bl
    jnz .err
    mov [number],edx
.format:
    push ds
    pop es
    cld
    mov di,realmode
    mov eax,'Real'
    stosd
    mov al,' '
    stosb
    call fmt
    mov di,standard
    mov eax,'Std '
    stosd
    call fmt
    mov di,enhanced
    mov eax,'386 '
    stosd
    call fmt
    mov ax,352fh
    int 21h
    mov [old2f],bx
    mov [old2f+2],es
    mov dx,handler
    mov ax,252fh
    int 21h
    mov dx,(last-$$+100h+15)/16
    mov ax,3100h
    int 21h
.err:
    mov ax,4c01h
    int 21h
