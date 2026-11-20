; Static Windows enhanced-mode VxD. All code/data stays resident.
bits 32
section .text align=4
global _WkDescriptor
extern _WkVxdStart, _WkVxdStop, _WkVxdOpen, _WkVxdClose, _WkVxdQuery
extern _WkTargetVersion, _WkMatchTarget
extern _WkRunEvent
extern _WkVxdStack, _WkVxdDeferred, _WkVxdPci
global _WkCreateSemaphore, _WkWaitSemaphore, _WkSignalSemaphore, _WkDestroySemaphore
global _WkYieldVM
global _WkGetSystemPages, _WkReturnSystemPages
global _WkMaskInterrupts, _WkRestoreInterrupts, _WkScheduleEvent, _WkCancelEvent
global @InterlockedExchange@8

_WkDescriptor:
    dd 0
    dw 0300h, 07bd1h
    db 0, 1
    dw 0
    db 'WDMKIT  '
    dd 80000000h, control, 0, pm_api
    dd 0, 0, 0, 0, 0

control:
    cmp eax, 23h                   ; W32_DEVICEIOCONTROL (Windows 9x)
    je .ioctl
    cmp eax, 5                     ; System_Exit
    je .shutdown
    cmp eax, 1                     ; Device_Init
    jne .ok
    pushad
    cld
    call _WkVxdStart
    test eax, eax
    jz .failed
    call _WkTargetVersion
    mov esi, compact_ready
    cmp eax, 0600h
    jne .ready_message
    mov esi, ready
.ready_message:
    call debug_out
    popad
.ok:
    clc
    ret
.failed:
    mov esi, failed
    call debug_out
    popad
    stc
    ret
.shutdown:
    pushad
    cld
    call _WkVxdStop
    popad
    clc
    ret
.ioctl:
    mov eax, [esi+12]               ; DIOCParams.dioc_IOCtlCode
    test eax, eax                   ; DIOC_Open
    jz .open
    cmp eax, -1                     ; DIOC_CloseHandle
    je .close
    cmp eax, 83372400h              ; six read-only target match controls
    jb .ordinary
    cmp eax, 83372414h
    ja .unknown
    pushad
    cld
    push eax
    call _WkMatchTarget
    add esp, 4
    jmp .result
.ordinary:
    cmp eax, 83372004h              ; WK_IOCTL_DEFERRED
    je .defer
    cmp eax, 8337200ch              ; WK_IOCTL_STACK
    je .stack
    cmp eax, 83372010h              ; WK_IOCTL_PCI
    je .pci
    cmp eax, 83372000h              ; WK_IOCTL
    jne .unknown
    pushad
    cld
    call _WkVxdQuery
    jmp .result
.defer:
    pushad
    cld
    call _WkVxdDeferred
    jmp .result
.stack:
    pushad
    cld
    call _WkVxdStack
    jmp .result
.pci:
    pushad
    cld
    call pci_query
    jmp .result
.open:
    pushad
    cld
    call _WkVxdOpen
    jmp .result
.close:
    pushad
    cld
    call _WkVxdClose
.result:
    test eax, eax
    popad
    jz .error
.success:
    xor eax, eax
    ret
.unknown:
    mov eax, 50                     ; ERROR_NOT_SUPPORTED
    ret
.error:
    mov eax, 31                     ; ERROR_GEN_FAILURE
    ret

; VMM supplies EBP = Client_Reg_Struc. Return the result through client AX.
; This entry point accepts no addresses or lengths from the caller.
pm_api:
    pushad
    cld
    cmp word [ebp+28], 5744h        ; return the compiled WDM target
    jne .test_pci
    call _WkTargetVersion
    jmp .reply
.test_pci:
    cmp word [ebp+28], 5743h
    jne .control
    call pci_query
    jmp .signature
.control:
    call _WkVxdQuery
    test eax, eax
    jz .reply
    call _WkVxdDeferred
.signature:
    test eax, eax
    jz .reply
    mov eax, 574bh
.reply:
    mov [ebp+28], ax                ; Client_AX
    popad
    ret

debug_out:
    cld
.next:
    lodsb
    test al, al
    jz .done
    out 0e9h, al                    ; QEMU isa-debugcon
    jmp .next
.done:
    ret

pci_query:
    call _WkVxdPci
    push eax
    mov esi, pci_failed
    test eax, eax
    jz .log
    mov esi, pci_passed
.log:
    call debug_out
    pop eax
    ret
_WkMaskInterrupts:
    pushfd
    cli
    pop eax
    test eax, 0200h
    setz al
    movzx eax, al
    add eax, eax                    ; enabled -> PASSIVE_LEVEL, masked -> DISPATCH_LEVEL
    ret

@InterlockedExchange@8:
    mov eax, edx
    xchg eax, [ecx]                 ; x86 memory XCHG is atomic on the 386
    ret

_WkRestoreInterrupts:
    cmp byte [esp+4], 0
    jne .done
    sti
.done:
    ret

_WkScheduleEvent:
    push esi
    mov esi, run_event
    xor edx, edx
    int 20h
    dd 0001000eh                    ; VMM Schedule_Global_Event (Windows 3.0+)
    mov eax, esi
    pop esi
    ret

_WkCancelEvent:
    push esi
    mov esi, [esp+8]
    int 20h
    dd 00010012h                    ; VMM Cancel_Global_Event
    pop esi
    ret

run_event:
    pushad
    cld
    call _WkRunEvent
    test eax, eax
    mov esi, passed
    jnz .log
    mov esi, failed
.log:
    call debug_out
    popad
    ret

_WkCreateSemaphore:
    xor ecx, ecx                    ; initial token count
    int 20h
    dd 00010025h
    jnc .done
    xor eax, eax
.done:
    ret
_WkWaitSemaphore:
    mov eax, [esp+4]
    mov ecx, 5                      ; Block_Svc_Ints | Block_Enable_Ints
    int 20h
    dd 00010027h
    ret
_WkSignalSemaphore:
    mov eax, [esp+4]
    int 20h
    dd 00010028h
    ret
_WkDestroySemaphore:
    mov eax, [esp+4]
    int 20h
    dd 00010026h
    ret
_WkYieldVM:
    int 20h
    dd 00010038h                    ; VMM Release_Time_Slice
    ret

_WkGetSystemPages:
    mov ecx, [esp+4]
    push dword 081h                 ; PageLocked | PageZeroInit
    push dword 0                    ; physical address output, unused
    push dword 0                    ; maximum physical page
    push dword 0                    ; minimum physical page
    push dword 0                    ; alignment mask
    push dword 0                    ; VM handle, zero for PG_SYS
    push dword 1                    ; PG_SYS
    push ecx
    int 20h
    dd 00010053h                    ; VMM _PageAllocate
    add esp, 32
    mov ecx, [esp+8]
    mov [ecx], eax                  ; handle for _PageFree
    test eax, eax
    jz .done
    mov eax, edx                    ; linear system address
.done:
    ret

_WkReturnSystemPages:
    mov eax, [esp+4]
    push dword 0
    push eax
    int 20h
    dd 00010055h                    ; VMM _PageFree
    add esp, 8
    ret

compact_ready db 'WINWDM: DriverEntry and control device ready, two-device stack verified',13,10,0
ready db 'WINWDM: DriverEntry and control device ready, seven-device stack and 5000-byte extension verified',13,10,0
passed db 'WINWDM: filter stack, deferred completion and cancellation PASS',13,10,0
failed db 'WINWDM: kernel check FAIL',13,10,0
pci_passed db 'WINWDM: QEMU PCI port counter 2, PnP and D3/D0 PASS, pending I/O drained on removal',13,10,0
pci_failed db 'WINWDM: QEMU PCI test device absent or test failed',13,10,0
