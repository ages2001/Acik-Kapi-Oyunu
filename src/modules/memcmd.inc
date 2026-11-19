; Preserve native MEM switches by executing the edition's original MEM.EXE.
command_start:
    mov sp,stacktop
    mov bx,(stacktop-$$+100h+15)/16
    mov ah,4ah
    int 21h
    jc run_error
    xor cx,cx
    mov cl,[80h]
    mov si,81h
.trim:
    jcxz summary
    lodsb
    cmp al,' '
    jne .options
    loop .trim
    jmp summary
.options:
    mov es,[2ch]
    xor di,di
    xor ax,ax
    mov cx,32767
.find:
    repne scasb
    jcxz run_error
    cmp byte [es:di],0
    jne .find
    add di,3
    mov si,executable
.copy:
    cmp si,executable+120
    jae run_error
    mov al,[es:di]
    inc di
    mov [si],al
    inc si
    or al,al
    jnz .copy
    cmp si,executable+5
    jb run_error
    cmp byte [si-5],'.'
    jne run_error
    mov dword [si-4],'EXE'
    mov ax,ds
    mov [params+4],ax
    mov [params+8],ax
    mov [params+12],ax
    push ds
    pop es
    mov bx,params
    mov dx,executable
    mov ax,4b00h
    int 21h
    jc run_error
    mov ah,4dh
    int 21h
    mov ah,4ch
    int 21h
run_error:
    mov dx,errormsg
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h
summary:
    int 12h
    movzx eax,ax
    push eax
    mov dx,base_label
    call puts
    pop eax
    call num
    mov dx,unit
    call puts
    ; DOS returned the size of the largest conventional allocation in paragraphs.
    mov bx,0xffff
    mov ah,48h
    int 21h
    jnc .xms
    movzx eax,bx
    shl eax,4
    push eax
    mov dx,block_label
    call puts
    pop eax
    call num
    mov dx,byte_label
    call puts
.xms:
    jmp report_xms
base_label db 'BIOS conventional memory: $'
block_label db 'Largest free DOS block: $'
byte_label db ' bytes',13,10,'$'
errormsg db 'Could not start the original MEM.EXE for this option.',13,10,'$'
params dw 0,80h,0,5ch,0,6ch,0
executable times 121 db 0
