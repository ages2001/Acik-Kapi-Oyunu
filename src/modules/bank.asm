; Shared high-memory storage for a single cooperative legacy Windows session.
; Reserve the high XMS block before the old protected-mode loader consumes XMS.
cpu 386
bits 16
org 100h
jmp install
%ifdef BANK_WINDOW
align 16
bank_header: db 'XBNK386!'
    dd 0,0
    dw buffer
%ifdef WIN2_APERTURE
    dw aperture_space
%else
    dw 0
%endif
%endif
free_kib dd 0
bitmap_offset dw usage_map
old2f dd 0
xms dd 0
low_handle dw 0
bank_handle dw 0
bank_bytes dd 0
physical dd 0
busy db 0
request: dd 0
source_h dw 0
source_pos dd 0
dest_h dw 0
dest_pos dd 0
handler:
    cmp ax,0xd74d
    jne .chain
    cmp bx,0x4258
    jne .chain
    or cx,cx
    jz near .query
    cmp cx,4
    je near .free
    cmp cx,5
    je near .range
    cmp cx,6
    je near .range
    cmp cx,3
    je near .physical
    cmp cx,2
    ja .error
    cmp byte [cs:busy],0
    jne .error
    cmp si,32768
    ja .error
    test si,1
    jnz .error
    movzx edx,dx
    movzx eax,bp
    shl eax,16
    or edx,eax
    movzx eax,si
    add eax,edx
    jc .error
    cmp eax,[cs:bank_bytes]
    ja .error
    mov byte [cs:busy],1
    push ds
    push es
    push si
    push di
    push bx
    push cs
    pop ds
    movzx eax,si
    mov [request],eax
    xor eax,eax
    mov ax,cs
    shl eax,16
    mov ax,buffer
    mov bx,[bank_handle]
    cmp cx,2
    je .write
    mov [source_h],bx
    mov [source_pos],edx
    mov word [dest_h],0
    mov [dest_pos],eax
    jmp .move
.write:
    mov word [source_h],0
    mov [source_pos],eax
    mov [dest_h],bx
    mov [dest_pos],edx
.move:
    push cx
    mov si,request
    mov ah,0bh
    call far [xms]
    pop cx
    test ax,ax
    jz .marked
    cmp cx,2
    jne .marked
    cmp word [request],0
    je .marked
    pushad
    mov eax,[dest_pos]
    mov edx,eax
    add edx,[request]
    dec edx
    shr eax,15
    shr edx,15
    call claim_one
    cmp eax,edx
    je .markdone
    mov eax,edx
    call claim_one
.markdone:
    popad
.marked:
    mov byte [busy],0
    pop bx
    pop di
    pop si
    pop es
    pop ds
    or ax,ax
    jz .error
    mov ax,0x5842
    iret
.error:
    xor ax,ax
    iret
.query:
    mov ax,0x5842
    mov bx,1
    mov edx,[cs:bank_bytes]
    shr edx,10
    mov ecx,edx
    shr ecx,16
    mov si,cs
    mov di,buffer
    iret
.free:
    mov edx,[cs:free_kib]
    mov ecx,edx
    shr ecx,16
    mov ax,0x5842
    iret
; Allocate or release an explicit run of 32 KiB units. EDX is the first
; unit, ESI the unit count. Validate the entire run before altering it.
.range:
    test esi,esi
    jz .error
    mov eax,edx
    add eax,esi
    jc .error
    mov ebx,[cs:bank_bytes]
    shr ebx,15
    cmp eax,ebx
    ja .error
    pushad
    mov edi,edx
    mov ebp,esi
.check:
    mov eax,edi
    call bit_address
    bt word [cs:usage_map+bx],ax
    setc al
    cmp cx,5
    jne .check_used
    test al,al
    jnz .invalid
    jmp short .checked
.check_used:
    test al,al
    jz .invalid
.checked:
    inc edi
    dec ebp
    jnz .check
    mov edi,edx
.update:
    mov eax,edi
    call bit_address
    cmp cx,5
    jne .release
    bts word [cs:usage_map+bx],ax
    sub dword [cs:free_kib],32
    jmp short .nextbit
.release:
    btr word [cs:usage_map+bx],ax
    add dword [cs:free_kib],32
.nextbit:
    inc edi
    dec esi
    jnz .update
    popad
    mov ax,0x5842
    iret
.invalid:
    popad
    jmp .error
.physical:
    mov edx,[cs:physical]
    mov ecx,edx
    shr ecx,16
    mov ax,0x5842
    iret
.chain:
    jmp far [cs:old2f]
; Resolve a 17-bit unit index without wrapping a 16-bit addressing mode.
bit_address:
    mov ebx,eax
    shr ebx,3
    and ax,7
    ret
claim_one:
    push eax
    call bit_address
    bts word [cs:usage_map+bx],ax
    jc .done
    sub dword [cs:free_kib],32
.done:
    pop eax
    ret
usage_map: times 16386 db 0
buffer: times 32768 db 0
%ifdef WIN2_APERTURE
aperture_space: times 8192 db 0
%endif
resident_end:
install:
    mov ax,0xd74d
    mov bx,0x4258
    xor cx,cx
    int 2fh
    cmp ax,0x5842
    je .already
    mov ax,4300h
    int 2fh
    cmp al,80h
    jne .fail
    mov ax,4310h
    int 2fh
    mov [xms],bx
    mov [xms+2],es
    mov ah,88h
    call far [xms]
    cmp eax,32768
    jb .fail
    ; HimemX2 takes this first allocation from low RAM. Return it to DOSX/Win386.
    mov edx,12288
    mov ah,89h
    call far [xms]
    or ax,ax
    jz .fail
    mov [low_handle],dx
%ifdef WIN2_APERTURE
    ; Leave the old VMM 12 MiB of XMS and keep a guard below the bank.
    mov edx,20480
    mov ah,89h
    call far [xms]
    test ax,ax
    jz .undo
%endif
    mov ah,88h
    call far [xms]
    and eax,-32
    mov [free_kib],eax
    mov edx,eax
    shl eax,10
    mov [bank_bytes],eax
    mov ah,89h
    call far [xms]
    test ax,ax
    jz .undo
    mov [bank_handle],dx
    mov ah,0ch
    call far [xms]
    test ax,ax
    jz .unlock_failed
    shl edx,16
    mov dx,bx
    mov [physical],edx
%ifdef BANK_WINDOW
    mov [bank_header+8],edx
    mov eax,[bank_bytes]
    mov [bank_header+12],eax
%endif
    mov dx,[low_handle]
    mov ah,0ah
    call far [xms]
    test ax,ax
    jz .fail
    mov ax,352fh
    int 21h
    mov [old2f],bx
    mov [old2f+2],es
    mov dx,handler
    mov ax,252fh
    int 21h
    mov dx,ready
    mov ah,9
    int 21h
    mov dx,(resident_end-$$+100h+15)/16
    mov ax,3100h
    int 21h
.unlock_failed:
    mov dx,[bank_handle]
    mov ah,0ah
    call far [xms]
.undo:
    mov dx,[low_handle]
    mov ah,0ah
    call far [xms]
.fail:
    mov dx,failure
    mov ah,9
    int 21h
    mov ax,4c01h
    int 21h
.already:
    mov ax,4c00h
    int 21h
ready db 'High XMS bank available; 12 MiB left for the legacy loader.',13,10,'$'
failure db 'High XMS bank unavailable.',13,10,'$'
