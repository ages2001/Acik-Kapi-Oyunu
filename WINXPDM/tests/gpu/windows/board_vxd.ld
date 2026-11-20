/* The LE packer derives the VMM relocations from two flat link addresses. */
OUTPUT_FORMAT(binary)
SECTIONS {
    . = image_base;
    .board : {
        *(.ddb) *(.text*) *(.rodata*) *(.data*) *(.bss*) *(COMMON)
        BYTE(0)
    }
    /DISCARD/ : { *(.note*) *(.comment) *(.eh_frame*) }
}
