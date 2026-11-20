bits 32
cpu 386
org 0
%define WINDOW 0xff800000
%define LOW_BYTES 0x3000000
%define TABLE 0xbab1e000         ; replaced by a locked-object LE relocation

; Convert a physical address to a temporary kernel alias. Each outstanding
; use owns one slot until release, including while a pager call blocks.
map:
    cmp eax,LOW_BYTES
    jae .high
    add eax,0x400000
    ret
.high:
    pushfd
    cli
    push ecx
    push edx
    push edi
    mov edi,[TABLE]
    xor ecx,ecx
.next:
    cmp dword [edi+ecx*4],0
    je short .found
    inc ecx
    cmp ecx,1024
    jb short .next
    int 20h
    dd 0x000100ae                 ; replaced with Fatal_Memory_Error from this build
.found:
    mov edx,eax
    and edx,-4096
    or edx,3                      ; present, supervisor, writable
    mov [edi+ecx*4],edx
    mov edx,cr3
    mov cr3,edx                   ; 386-compatible TLB flush
    shl ecx,12
    and eax,4095
    lea eax,[eax+ecx+WINDOW]
    pop edi
    pop edx
    pop ecx
    popfd
    ret

release:
    pushfd
    cmp eax,WINDOW
    jb short .out
    cmp eax,WINDOW+0x400000
    jae short .out
    cli
    push eax
    push edi
    sub eax,WINDOW
    shr eax,12
    mov edi,[TABLE]
    mov dword [edi+eax*4],0
    mov eax,cr3
    mov cr3,eax
    pop edi
    pop eax
.out:
    popfd
    ret

page_out:
    push esi
    push eax
    mov eax,ecx                   ; PageSwap receives a stable buffer until return
    call map
    mov esi,eax
    pop eax
    push esi
    int 20h
    dd 0x00070005
    jmp short pager_return
page_in:
    push edi
    push eax
    mov eax,ecx
    call map
    mov edi,eax
    pop eax
    push edi
    int 20h
    dd 0x00070004
    pop edi
    xchg edi,eax
    call release
    xchg edi,eax
    pop edi
    ret
pager_return:
    pop esi
    xchg esi,eax
    call release
    xchg esi,eax
    pop esi
    ret

read_dword:                      ; EDX = guest physical address -> DWORD
    push eax
    mov eax,edx
    call map
    mov edx,[eax]
    call release
    pop eax
    ret
read_fault_entry:
    call read_dword
    mov esi,edx
    lea edi,[esp+4]
    ret
copy_entries:                    ; CopyPageTable transfers one PT span
    push eax
    mov eax,edx
    and eax,-4096
    call map
    add esi,eax
    cld
    rep movsd
    call release
    pop eax
    ret

map_phys_enter:
    enter 0x18,0
    mov dword [ebp-0x18],0
    push esi
    push edi
    push ebx
    db 0xe9
resume_operand: dd 0              ; linked to the validated MapPhysToLinear continuation
map_phys_table:
    push eax
    mov eax,[ebp-0x18]
    call release
    mov eax,edx
    call map
    mov edx,eax
    mov [ebp-0x18],eax
    pop eax
    ret
map_phys_exit:
    push eax
    mov eax,[ebp-0x18]
    call release
    pop eax
    pop ebx
    pop edi
    pop esi
    leave
    ret
symbols:
dd page_out,page_in,read_dword,read_fault_entry,copy_entries,map_phys_enter,map_phys_table,map_phys_exit,resume_operand
