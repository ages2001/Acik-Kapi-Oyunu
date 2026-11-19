; Windows 3.0 enhanced-mode transport for the DOS XMS bank. Only requests
; inside the boot-time reserved block from the system VM are accepted.
bits 32
cpu 386
segment _LTEXT public class=LCODE use32
global BANKWIN_DDB
BANKWIN_DDB:
    dd 0
    dw 0x0300,0
    db 1,0
    dw 0
    db 'BANKWIN '
    dd 0x80000000,control,0,0,0,0,0,0,0
%macro vmm 1
    int 20h
    dd 0x00010000 + %1
%endmacro
control:
    cmp eax,1
    jne .ok
    pushad
    mov [sysvm],ebx
    mov edi,[ebx+4]                ; CB_High_Linear
    add edi,0x510
    mov ecx,(0xa0000-0x510-64)/16
.find:
    cmp dword [edi],'XBNK'
    jne .next
    cmp dword [edi+4],'386!'
    je .found
.next:
    add edi,16
    loop .find
    jmp .end
.found:
    mov esi,[edi+8]                ; physical base
    mov ecx,[edi+12]               ; reserved bytes
    mov edx,esi
    add edx,ecx
    jc .end
    cmp esi,0x100000
    jb .end
    test esi,4095
    jnz .end
    test ecx,4095
    jnz .end
    mov [bank_start],esi
    mov [bank_size],ecx
    lea eax,[edi+20]
    mov [free_address],eax
    movzx eax,word [edi+24]
    lea eax,[edi+eax-0x110]
    mov [usage_address],eax
    movzx eax,word [edi+16]        ; transfer buffer offset from PSP
    sub edi,0x110
    add eax,edi
    mov [transfer],eax
    mov eax,cr3
    and eax,-4096
    call map_phys
    test eax,eax
    jz .end
    mov edx,window_space
    add edx,4095
    and edx,-4096
    mov [window],edx
    mov ecx,edx
    shr ecx,22
    mov eax,[eax+ecx*4]
    test al,1
    jz .end
    and eax,-4096
    call map_phys
    test eax,eax
    jz .end
    mov edx,[window]
    shr edx,12
    and edx,1023
    lea eax,[eax+edx*4]
    mov [window_pte],eax
    mov eax,2fh
    mov esi,bank_interrupt
    vmm 0x41                        ; Hook_V86_Int_Chain
    jc .end
    mov al,'V'
    out 0xe9,al
.end:
    popad
.ok:
    clc
    ret
map_phys:
    push byte 0
    push dword 4096
    push eax
    vmm 0x6c                        ; _MapPhysToLinear(address, size, flags)
    add esp,12
    cmp eax,-1
    jne .ret
    xor eax,eax
.ret:
    ret
bank_interrupt:
    cmp ebx,[sysvm]
    jne .chain
    cmp word [ebp+28],0xd74d
    jne .chain
    cmp word [ebp+16],0x4258
    jne .chain
    cmp word [ebp+24],1
    jb .chain
    cmp word [ebp+24],2
    ja .chain
    pushad
    movzx eax,word [ebp+8]
    shl eax,16
    mov ax,[ebp+20]                 ; 32-bit byte position
    movzx ecx,word [ebp+4]          ; transfer size
    cmp ecx,32768
    ja .bad
    mov edx,eax
    add edx,ecx
    jc .bad
    cmp edx,[bank_size]
    ja .bad
    add eax,[bank_start]
    mov edx,[transfer]
.loop:
    test ecx,ecx
    jz .good
    push ecx
    mov edi,eax
    and edi,4095
    mov esi,4096
    sub esi,edi
    cmp ecx,esi
    jbe .chunk
    mov ecx,esi
.chunk:
    pushfd
    cli
    push eax
    mov esi,[window_pte]
    push dword [esi]
    and eax,-4096
    or eax,3
    mov [esi],eax
    mov eax,cr3
    mov cr3,eax
    add edi,[window]
    mov esi,edx
    cmp word [ebp+24],2
    je .write
    xchg esi,edi
.write:
    cld
    push ecx
    push ecx
    shr ecx,2
    rep movsd
    pop ecx
    and ecx,3
    rep movsb
    pop ecx
    mov esi,[window_pte]
    pop dword [esi]
    mov eax,cr3
    mov cr3,eax
    pop eax
    popfd
    add eax,ecx
    add edx,ecx
    mov esi,ecx
    pop ecx
    sub ecx,esi
    jmp .loop
.good:
    cmp word [ebp+24],2
    jne .accounted
    movzx ecx,word [ebp+4]
    test ecx,ecx
    jz .accounted
    movzx eax,word [ebp+8]
    shl eax,16
    mov ax,[ebp+20]
    lea edx,[eax+ecx-1]
    shr eax,15
    shr edx,15
    mov esi,[usage_address]
    mov edi,[free_address]
    bts dword [esi],eax
    jc .last
    sub dword [edi],32
.last:
    bts dword [esi],edx
    jc .accounted
    sub dword [edi],32
.accounted:
    mov word [ebp+28],0x5842
    jmp .handled
.bad:
    mov word [ebp+28],0
.handled:
    popad
    clc                             ; consumed the virtual interrupt
    ret
.chain:
    stc
    ret
align 4
sysvm dd 0
bank_start dd 0
bank_size dd 0
free_address dd 0
usage_address dd 0
transfer dd 0
window dd 0
window_pte dd 0
; The complete window is on its own locked page, independent of LE placement.
window_space: times 8192 db 0
