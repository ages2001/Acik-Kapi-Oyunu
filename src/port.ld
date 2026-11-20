ENTRY(port_init)
SECTIONS {
    . = 0;
    .text : { *(.text*) *(.rodata*) *(.got*) }
    . = ALIGN(4096);
    data_start = .;
    .data : { *(.data*) *(.bss*) BYTE(0) }
    /DISCARD/ : { *(.comment*) *(.note*) *(.eh_frame*) }
}
