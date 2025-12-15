ENTRY(_start)
OUTPUT_FORMAT(binary)

SECTIONS {
    . = 0x0;
    
    .text : ALIGN(16) {
        *(.text*)
    }
    
    .rodata : ALIGN(16) {
        *(.rodata*)
    }
    
    .data : ALIGN(16) {
        *(.data*)
    }
    
    .bss : ALIGN(16) {
        *(.bss*)
        *(COMMON)
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}