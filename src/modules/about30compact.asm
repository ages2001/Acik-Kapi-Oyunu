bits 16
cpu 386
org BEGIN_AT
; The original code initialized the two EMS locals (and the protected-mode
; flag to zero) above this point. LoadString is the fallback on a stock boot.
mov dx,ax
and ax,0x300
cwde
mov [bp-0x5a],eax
mov ax,dx
and ax,1
mov [bp-0x5c],ax
push si
push di
mov ax,0xd74f
mov bx,0x5854
int 2fh
cmp ax,0x5842
jne fallback
mov [bp-0x4e],ebx
mov [bp-0x4a],ecx
mov [bp-0x46],edx
mov [bp-0x42],esi
mov [bp-0x3e],edi
mov byte [bp-0x3a],0
pop di
pop si
jmp short SET_TEXT
fallback:
pop di
pop si
mov ax,FIRST_ID
test dl,1
jz finish
add al,3
test dl,0x10
jnz finish
inc ax
finish:
mov [bp+0xa],ax
