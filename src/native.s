/* Each fixed SCSI import thunk uses the native loader's resolved IAT slot. */
.text
.macro native name
.globl \name
.type \name,@function
\name:
call 1f
1: popl %eax
.byte 0xff,0xa0
.long 0
.endm
native base_init
native base_read
native base_map
native base_uncached
native base_write
.section .note.GNU-stack,"",@progbits
