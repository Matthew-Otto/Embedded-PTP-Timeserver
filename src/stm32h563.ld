/* STM32H563ZIT6U Linker Script */

ENTRY(reset_handler)

/* Memories definition */
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 2M
    SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 640K
}

/* Configure Stack */
_estack = ORIGIN(SRAM) + LENGTH(SRAM); /* top of SRAM */
_stack_size = 4K;
_stack_top = _estack;
_stack_bottom = _estack - _stack_size;

SECTIONS
{
    /* Vector table */
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    .text : {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* Used by the startup to initialize data */
    _sidata = LOADADDR(.data);

    .data : {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > SRAM AT > FLASH

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM

    .pkt_buffer_pool (NOLOAD) : ALIGN(4) {
        KEEP(*(.pkt_buffer_pool))
    } > SRAM

    .heap (NOLOAD) : ALIGN(4) {
        KEEP(*(.heap))
    } > SRAM

    _end = .;
}
