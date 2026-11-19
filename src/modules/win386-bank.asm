; Enhanced memory transport for the Windows/386 2.11 X.OUT virtual monitor.
; The system VM owns a conventional transfer buffer and a second aligned
; page. Its normal VM page table supplies the alias, which is restored before
; interrupts resume. The preboot XMS handle remains locked for the session.
bits 32
cpu 386
%include "xout-layout.inc"
org BANK_CODE
entry:
    cmp word [ebp+28],0xd74d
    jne near legacy
    cmp word [ebp+16],0x4258
    jne near legacy
    cmp dword [CUR_VMID],1
    jne near legacy
    cmp word [ebp+24],1
    jb near legacy
    cmp word [ebp+24],2
    ja near legacy
    pushad
    push es
    push ds
    pop es
    cmp dword [bank_start],0
    jne transfer
    mov edi,0x510
.find:
    cmp dword [edi],'XBNK'
    jne .next
    cmp dword [edi+4],'386!'
    je .found
.next:
    add edi,16
    cmp edi,0x9ffb0
    jb .find
    jmp failed
.found:
    mov esi,[edi+8]
    mov ecx,[edi+12]
    mov edx,esi
    add edx,ecx
    jc failed
    cmp esi,0x2000000
    jb failed
    test esi,4095
    jnz failed
    test ecx,4095
    jnz failed
    lea eax,[edi+20]
    mov [free_address],eax
    movzx eax,word [edi+24]
    lea eax,[edi+eax-0x110]
    mov [usage_address],eax
    sub edi,0x110
    movzx eax,word [edi+0x120]
    add eax,edi
    mov [buffer],eax
    movzx eax,word [edi+0x122]
    lea eax,[eax+edi+4095]
    and eax,-4096
    cmp eax,0x9f000
    ja failed
    mov [window],eax
    mov [bank_size],ecx
    mov [bank_start],esi
transfer:
    movzx eax,word [ebp+8]
    shl eax,16
    mov ax,[ebp+20]
    movzx ecx,word [ebp+4]
    cmp ecx,32768
    ja failed
    mov edx,eax
    add edx,ecx
    jc failed
    cmp edx,[bank_size]
    ja failed
    add eax,[bank_start]
    mov edx,[buffer]
    mov esi,[CUR_VM_HANDLE]
    mov esi,[esi]
    mov edi,[window]
    shr edi,12
    lea esi,[esi+edi*4]
    mov [window_pte],esi
.loop:
    test ecx,ecx
    jz success
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
success:
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
    jmp finish
failed:
    mov word [ebp+28],0
finish:
    pop es
    popad
    jmp EVENT_CHK
legacy:
    mov eax,2fh
    jmp LEGACY_RESUME
align 4
bank_start dd 0
bank_size dd 0
free_address dd 0
usage_address dd 0
buffer dd 0
window dd 0
window_pte dd 0
