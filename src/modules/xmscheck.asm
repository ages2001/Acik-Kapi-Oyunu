; NASM 16-bit DOS program, 80386 instruction set, public XMS 3.0 interface.
cpu 386
bits 16
%ifdef WIN_CLIENT
segment _TEXT public class=CODE use16
global XMSCHECK
XMSCHECK:
    pushad
    push es
    mov word [count],0
    mov dword [total_k],0
    mov dword [highest],0
    mov byte [status],0
%else
org 100h
%endif
start:
    mov [convseg],ds
    mov dx,header
    call puts
%ifndef WIN_CLIENT
    mov ax,3306h
    int 21h
    movzx eax,bl
    call number
    mov dl,'.'
    call char
    movzx eax,bh
    call number
%endif
    mov dx,query
    call puts
    mov ax,4300h
    int 2fh
    cmp al,80h
    jne fail
    mov ax,4310h
    int 2fh
    mov [driver],bx
    mov [driver+2],es
    xor ah,ah
    call far [driver]
    cmp ax,300h
    jb fail
    mov ah,88h
    call far [driver]
    mov [expected_k],edx
    mov eax,edx
    call number
    mov dx,kib
    call puts
allocate:
    cmp word [count],66
    jae fail
    mov ah,88h
    call far [driver]
    and eax,-32
    jz allocated
    cmp eax,262144
    jbe size_ok
    mov eax,262144
size_ok:
    mov [size_k],eax
    mov edx,eax
    mov ah,89h
    call far [driver]
    test ax,ax
    jz fail
    mov bx,[count]
    shl bx,3
    mov [blocks+bx],dx
    mov eax,[size_k]
    add [total_k],eax
    shl eax,10
    mov [blocks+bx+4],eax
    inc word [count]
    mov ah,0ch
    call far [driver]
    test ax,ax
    jz fail
    shl edx,16
    mov dx,bx
    mov eax,[size_k]
    shl eax,10
    lea eax,[edx+eax-1]
    cmp [highest],eax
    jae skip_last
    mov [highest],eax
skip_last:
    mov bx,[count]
    dec bx
    shl bx,3
    mov dx,[blocks+bx]
    mov ah,0dh
    call far [driver]
    test ax,ax
    jz fail
    jmp allocate
allocated:
    cmp word [count],0
    je fail
    mov dx,allocated_text
    call puts
    mov eax,[total_k]
    call number
    mov dx,kib
    call puts
    mov dx,highest_text
    call puts
    mov eax,[highest]
    call number
    mov dx,newline
    call puts
    mov eax,[expected_k]
    sub eax,[total_k]
    cmp eax,32
    jae fail
    mov word [reading],0
pass:
    xor bx,bx
    mov [block_i],bx
block:
    mov bx,[block_i]
    cmp bx,[count]
    jae next_pass
    shl bx,3
    mov ax,[blocks+bx]
    mov [handle],ax
    mov eax,[blocks+bx+4]
    mov [block_bytes],eax
    mov dword [offset32],0
chunk:
    mov edx,[offset32]
    cmp edx,[block_bytes]
    jae next_block
    movzx eax,word [block_i]
    shl eax,16
    shr edx,12
    add eax,edx
    or eax,31000000h
    mov [tag],eax
    mov di,buffer
    mov cx,8
fill:
    mov edx,eax
    cmp word [reading],0
    je write_tag
    xor edx,edx
write_tag:
    mov [di],edx
    not edx
    mov [di+4092],edx
    add di,4096
    inc eax
    loop fill
    movzx eax,word [convseg]   ; segment:offset of conventional buffer
    shl eax,16
    mov ax,buffer
    mov bx,[handle]
    mov edx,[offset32]
    mov word [src_h],0
    mov [src_off],eax
    mov [dst_h],bx
    mov [dst_off],edx
    cmp word [reading],0
    je do_move
    mov [src_h],bx
    mov [src_off],edx
    mov word [dst_h],0
    mov [dst_off],eax
do_move:
    mov si,move_req
    mov ah,0bh
    call far [driver]
    test ax,ax
    jz fail
    cmp word [reading],0
    je next_chunk
    mov eax,[tag]
    mov di,buffer
    mov cx,8
check:
    cmp [di],eax
    jne fail
    not eax
    cmp [di+4092],eax
    jne fail
    not eax
    inc eax
    add di,4096
    loop check
next_chunk:
    add dword [offset32],32768
    jmp chunk
next_block:
    inc word [block_i]
    jmp block
next_pass:
    inc word [reading]
    cmp word [reading],2
    jb pass
    mov dx,verified
    call puts
    jmp cleanup
fail:
    mov byte [status],1
    mov dx,failed
    call puts
cleanup:
    cmp word [count],0
    je finish
    dec word [count]
    mov bx,[count]
    shl bx,3
    mov dx,[blocks+bx]
    mov ah,0ah
    call far [driver]
    test ax,ax
    jnz cleanup
    mov byte [status],1
    jmp cleanup
finish:
    cmp byte [status],0
    jne exit_bad
    mov dx,passed
    call puts
exit_bad:
    mov dx,ended
    call puts
%ifdef WIN_CLIENT
    pop es
    popad
    xor ax,ax
    mov al,[status]
    retf
%else
    mov ah,4ch
    mov al,[status]
    int 21h
%endif
puts:
    push ax
    push dx
    push si
    mov si,dx
.next:
    lodsb
    test al,al
    jz .end
    mov dl,al
    call char
    jmp .next
.end:
    pop si
    pop dx
    pop ax
    ret
char:
    pushad
    mov al,dl
    out 0e9h,al
%ifndef WIN_CLIENT
    mov ah,02h
    int 21h
%endif
    popad
    ret
number:
    pushad
    xor cx,cx
    mov ebx,10
.nd:
    xor edx,edx
    div ebx
    push dx
    inc cx
    test eax,eax
    jnz .nd
.emit:
    pop dx
    add dl,'0'
    call char
    loop .emit
    popad
    ret
%ifdef WIN_CLIENT
segment _DATA public class=DATA use16
group DGROUP _DATA
header db 'Native Win16 XMS test',13,10,0
%else
header db 'W31PROBE begin',13,10,'DOS ',0
%endif
query db 13,10,'XMS free before allocation: ',0
kib db ' KiB',13,10,0
allocated_text db 'Simultaneously allocated: ',0
highest_text db 'Highest locked physical byte: ',0
newline db 13,10,0
verified db 'Every 4 KiB page verified.',13,10,0
failed db 'result=FAIL',13,10,0
passed db 'result=PASS',13,10,0
ended db 'W31PROBE end',13,10,0
driver dd 0
convseg dw 0
count dw 0
block_i dw 0
reading dw 0
expected_k dd 0
size_k dd 0
total_k dd 0
highest dd 0
offset32 dd 0
tag dd 0
block_bytes dd 0
handle dw 0
status db 0
move_req dd 32768
src_h dw 0
src_off dd 0
dst_h dw 0
dst_off dd 0
blocks times 66*8 db 0
%ifdef WIN_CLIENT
buffer times 32768 db 0
%else
buffer equ $
%endif
