; Run WINRAM with DOS standard error redirected to the log on standard output.
; SPDX-License-Identifier: GPL-3.0-or-later
bits 16
org 100h
    mov ax, cs
    cli
    mov ss, ax
    mov sp, stack_end
    sti
    mov bx, (allocation_size + 15) / 16
    mov ah, 4ah
    int 21h
    jc failed
    mov bx, 1
    mov cx, 2
    mov ah, 46h
    int 21h
    jc failed
    mov ax, cs
    mov [tail_segment], ax
    mov [fcb1_segment], ax
    mov [fcb2_segment], ax
    mov es, ax
    mov dx, winram
    mov bx, parameters
    mov ax, 4b00h
    int 21h
    jc failed
    mov ah, 4dh
    int 21h
    mov ah, 4ch
    int 21h
failed:
    mov ax, 4c7fh
    int 21h
winram db 'C:\WINRAM.EXE', 0
parameters dw 0, 80h
tail_segment dw 0, 5ch
fcb1_segment dw 0, 6ch
fcb2_segment dw 0
times 256 db 0
stack_end:
allocation_size equ $-$$+100h
