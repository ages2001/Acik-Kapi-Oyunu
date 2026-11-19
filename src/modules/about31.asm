bits 16
org 0xdc0
pushf
test word [bp-0xd6],0x20
jz near standard
pushad
push es
sub sp,48
mov di,sp
push ss
pop es
mov ax,0x0500
int 0x31
jc short done
mov eax,[es:di+24]
cmp eax,0xfffff
ja short done
shl eax,2                         ; total physical pages -> KiB
lea di,[bp-0x116]
mov cx,32
xor bx,bx
find_end:
cmp byte [es:di],0
je short append
inc di
loop find_end
jmp short done
append:
mov dword [es:di],'    '
add di,4
mov ebx,10
xor cx,cx
digits:
xor edx,edx
div ebx
add dl,'0'
push dx
inc cx
test eax,eax
jnz short digits
mov bx,cx
emit:
pop ax
stosb
dec bx
jz short suffix
cmp bx,6
je short comma
cmp bx,3
jne short emit
comma:
mov al,','
stosb
jmp short emit
suffix:
mov dword [es:di],' KB '
mov dword [es:di+4],'RAM'
done:
add sp,48
pop es
popad
jmp short resume
standard:
pushad
mov dx,[bp-0xd6]
test dl,0x10
jz std_done
mov ax,0xd74f
mov bx,0x5854
int 2fh
cmp ax,0x5842
jne std_done
mov [bp-0x116],ebx
mov [bp-0x112],ecx
mov [bp-0x10e],edx
mov [bp-0x10a],esi
mov [bp-0x106],edi
mov byte [bp-0x102],0
std_done:
popad
resume:
popf
push word [bp+0xe]
push byte 0x66
jmp 0x03d2
