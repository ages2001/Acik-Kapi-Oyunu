cpu 386
bits 16
org 100h
%ifdef MEM_COMMAND
jmp command_start
%include "memcmd.inc"
%endif
report_xms:
mov ax,4300h
int 2fh
cmp al,80h
jne unavailable
mov ax,4310h
int 2fh
mov [driver],bx
mov [driver+2],es
xor ah,ah
call far [driver]
cmp ax,300h
jb unavailable
mov ah,88h
call far [driver]
test bl,bl
jnz unavailable
mov [largest],eax
mov [xmsfree],edx
mov [highest],ecx
mov dx,labfree
call puts
mov eax,[xmsfree]
call num
mov dx,unit
call puts
mov dx,lablarge
call puts
mov eax,[largest]
call num
mov dx,unit
call puts
mov dx,labtop
call puts
mov eax,[highest]
mov edx,eax
shr edx,10
inc edx
cmp edx,[xmsfree]
jae .address_ok
mov dx,unknown_address
call puts
jmp short .address_done
.address_ok:
call num
.address_done:
mov dx,crlf
call puts
push ds
pop es
xor ebx,ebx
.map:
mov di,entry
mov eax,0xe820
mov edx,0x534d4150
mov ecx,24
mov dword [di+20],1
int 15h
jc .end
cmp eax,0x534d4150
jne .end
cmp ecx,20
jb .end
cmp dword [di+16],1
jne .more
mov eax,[di+8]
add [usable],eax
mov eax,[di+12]
adc [usable+4],eax
.more:
test ebx,ebx
jnz .map
mov dx,labbios
call puts
mov eax,[usable]
mov edx,[usable+4]
shrd eax,edx,10
call num
mov dx,unit
call puts
.end:
mov ax,4c00h
int 21h
unavailable:
mov dx,missing
call puts
mov ax,4c01h
int 21h
num:
mov ebx,10
xor cx,cx
.loop:
xor edx,edx
div ebx
push dx
inc cx
test eax,eax
jnz .loop
.out:
pop dx
add dl,'0'
mov ah,2
int 21h
loop .out
ret
puts:
mov ah,9
int 21h
ret
unknown_address db 'unreported by XMS driver','$'
driver dd 0
largest dd 0
highest dd 0
xmsfree dd 0
usable dq 0
entry times 24 db 0
labfree db 'Free extended memory (XMS 3): $'
lablarge db 'Largest free XMS block: $'
labtop db 'Highest XMS physical byte address: $'
labbios db 'BIOS E820 usable RAM (type 1): $'
unit db ' KiB',13,10,'$'
crlf db 13,10,'$'
missing db 'XMS 3.0 memory manager unavailable.',13,10,'$'

%ifdef MEM_COMMAND
align 2
times 512 db 0
stacktop:
%endif
