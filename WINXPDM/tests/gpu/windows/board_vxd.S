/* Static enhanced-mode VxD, SDK 3.0. SPDX-License-Identifier: GPL-3.0-or-later */
.section .ddb,"a"
    .long 0
    .short 0x300, 0
    .byte 1, 0
    .short 0
    .ascii "WXPBOARD"
    .long 0x27ffffff, control
    .fill 7,4,0

.text
control:
    cmpl $1,%eax                 /* Device_Init */
    jne 1f
    pushal
    cld
    call wxp_board_device_init
    popal
1:  clc
    ret

.globl wxp_map_vmm_page
wxp_map_vmm_page:
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    movl 20(%esp),%eax
    pushl $0
    pushl $4096
    pushl %eax
    int $0x20
    .long 0x0001006c             /* VMM _MapPhysToLinear */
    addl $12,%esp
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    ret
.section .note.GNU-stack,"",@progbits
