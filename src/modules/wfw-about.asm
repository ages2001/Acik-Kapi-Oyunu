bits 16
org 0xdc0
pushf
pushad
push es
sub sp,80
mov di,sp
push ss
pop es
mov ax,0x0500
int 0x31
jc short done
mov di,sp
mov eax,[es:di+24]
cmp eax,0xfffff
ja short done
shl eax,2
mov edx,eax
shr edx,16
; Use SHELL's own locale-aware 32-bit integer formatter.
push ss
push di
push dx
push ax
call 0x400
mov si,di
findend:
cmp byte [ss:si],0
je short suffix
inc si
jmp short findend
suffix:
mov dword [ss:si],' KB '
mov dword [ss:si+4],'RAM'
push word [bp+8]
push byte 0x72
push ss
push di
db 0x9a
dw 0xffff,0
done:
add sp,80
pop es
popad
popf
lea ax,[bp-0x68]
push ss
push ax
jmp 0x2dd
