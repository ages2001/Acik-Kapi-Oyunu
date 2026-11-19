bits 16
cpu 386
org 0xdc0
pushf
pushad
mov dx,[bp-0xd6]
test dl,0x10
jz done
mov ax,0xd74f
mov bx,0x5854
int 2fh
cmp ax,0x5842
jne done
mov [bp-0x116],ebx
mov [bp-0x112],ecx
mov [bp-0x10e],edx
mov [bp-0x10a],esi
mov [bp-0x106],edi
mov byte [bp-0x102],0
done:
popad
popf
push word [bp+0xe]
push byte 0x66
jmp 0x03d2
