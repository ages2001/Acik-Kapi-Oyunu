; A small read-only SETVER profile reader for systems without Microsoft's driver.
; The module begins with the DOS SETVERXX device header and its table descriptor.
; WINSTART consumes the same table format used by MS-DOS SETVER.EXE.
bits 16
org 0
mz: db 'MZ'
    dw (end-$$) & 511,(end-$$+511)/512,0,2,0x10,0xffff,0,0xfffe,0,entry-base,0,0x1c,0,0,0
base:
    dd -1
    dw 0x8000,0,0
    db 'SETVERXX'
    dw 0x0100
    dd table-base
    dw 256
entry:
    push cs
    pop ds
    mov dx,message-base
    mov ah,9
    int 21h
    mov dl,[table-base+13]
    add dl,'0'
    mov ah,2
    int 21h
    mov dl,'.'
    int 21h
    mov al,[table-base+14]
    aam
    mov bx,ax
    mov dl,bh
    add dl,'0'
    mov ah,2
    int 21h
    mov dl,bl
    add dl,'0'
    int 21h
    mov dx,newline-base
    mov ah,9
    int 21h
    mov ax,4c00h
    int 21h
message db 'WINRAM local SETVER profile',13,10,'WINSTART.COM  $'
newline db 13,10,'$'
table db 12,'WINSTART.COM',3,30,0
    times 256-($-table) db 0
end:
