cpu 286
bits 16
org 100h
; Measure actual simultaneous DOS opens. Each open is a separate handle.
; AH=67h resizes the process table. FILES= bounds the system table.
mov sp,stacktop
mov bx,(stacktop-$$+100h+15)/16
mov ah,4ah
int 21h
jc fail
mov bx,255
mov ah,67h
int 21h
jc fail
mov dx,name
xor cx,cx
mov ah,5ah
int 21h
jc fail
mov bx,ax
mov ah,3eh
int 21h
.open:
mov dx,name
mov ax,3d00h
int 21h
jc report
mov bx,[used]
shl bx,1
mov [list+bx],ax
inc word [used]
cmp word [used],255
jb .open
report:
mov dx,msg
call display
mov ax,[used]
xor cx,cx
mov bx,10
.digit:
xor dx,dx
div bx
push dx
inc cx
or ax,ax
jnz .digit
.print:
pop ax
add al,'0'
out 0e9h,al
mov dl,al
mov ah,02h
int 21h
loop .print
mov dx,crlf
call display
.close:
cmp word [used],0
je success
dec word [used]
mov bx,[used]
shl bx,1
mov bx,[list+bx]
mov ah,3eh
int 21h
jc fail
jmp .close
success:
mov dx,name
mov ah,41h
int 21h
mov ax,4c00h
int 21h
fail:
mov dx,err
call display
mov ax,4c01h
int 21h
display:
push si
mov si,dx
.more:
lodsb
or al,al
jz .end
out 0e9h,al
mov dl,al
mov ah,02h
int 21h
jmp .more
.end:
pop si
ret
used dw 0
name db '.',92,0
times 128 db 0
msg db 'Additional simultaneous DOS file handles = ',0
crlf db 13,10,0
err db 'file handle test failed',13,10,0
list times 255 dw 0
times 512 db 0
stacktop:
