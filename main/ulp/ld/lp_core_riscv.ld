/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "sdkconfig.h"
#include "soc/soc.h"
#include "ld.common"

#if CONFIG_ESP_ROM_HAS_LP_ROM
/* With LP-ROM memory layout is different due to LP ROM stack/data */
#define ULP_MEM_START_ADDRESS SOC_RTC_DRAM_LOW + RESERVE_RTC_MEM
#else
#define ULP_MEM_START_ADDRESS (SOC_RTC_DRAM_LOW)
#endif

#undef ALIGNED_SYMBOL
#define ALIGNED_SYMBOL(X, SYMBOL) \
        . = ALIGN(X);             \
        SYMBOL = ABSOLUTE(.);

#define ALIGN_DOWN(SIZE, AL)   (SIZE & ~(AL - 1))
/* Ensure the end where the shared memory starts is aligned to 8 bytes
   if updating this also update the same in ulp_lp_core_memory_shared.c
 */
#define ALIGNED_COPROC_MEM ALIGN_DOWN(CONFIG_ULP_COPROC_RESERVE_MEM, 0x8)

ENTRY(reset_vector)

MEMORY
{
    /*first 128byte for exception/interrupt vectors*/
    vector_table(RX) :   ORIGIN = ULP_MEM_START_ADDRESS , LENGTH = 0x80
    ram(RWX) :           ORIGIN = ULP_MEM_START_ADDRESS + 0x80, LENGTH = ALIGNED_COPROC_MEM - 0x80 - CONFIG_ULP_SHARED_MEM
    shared_mem_ram(RW) : ORIGIN = ULP_MEM_START_ADDRESS + ALIGNED_COPROC_MEM - CONFIG_ULP_SHARED_MEM, LENGTH = CONFIG_ULP_SHARED_MEM
}

SECTIONS
{
    .vector.text :
    {
        /*exception/interrupt vectors*/
        __mtvec_base = .;
        KEEP (*(.init.vector .init.vector.*))
    } > vector_table

    . = ORIGIN(ram);

    .text ALIGN(4):
    {
        *(.text.vectors) /* Default reset vector must link to offset 0x80 */
        *(.text)
        *(.text*)
    } >ram

    .rodata ALIGN(4):
    {
        PROVIDE (__global_pointer$ = . + 0x800);

        /**
        * C++ constructor tables.
        *
        * Excluding crtbegin.o/crtend.o since IDF doesn't use the toolchain crt.
        *
        * RISC-V gcc is configured with --enable-initfini-array so it emits
        * .init_array section instead. But the init_priority sections will be
        * sorted for iteration in ascending order during startup.
        * The rest of the init_array sections is sorted for iteration in descending
        * order during startup, however. Hence a different section is generated for
        * the init_priority functions which is iterated in ascending order during
        * startup. The corresponding code can be found in startup.c.
        */
        ALIGNED_SYMBOL(4, __init_priority_array_start);
        KEEP (*(EXCLUDE_FILE (*crtend.* *crtbegin.*) .init_array.*));
        __init_priority_array_end = ABSOLUTE(.);

        ALIGNED_SYMBOL(4, __init_array_start);
        KEEP (*(EXCLUDE_FILE (*crtend.* *crtbegin.*) .init_array));
        __init_array_end = ABSOLUTE(.);

        ALIGNED_SYMBOL(4, __fini_array_start);
        KEEP (*(.fini_array));
        __fini_array_end = ABSOLUTE(.);

        ALIGNED_SYMBOL(4, __fini_priority_array_start);
        KEEP (*(.fini_array.*));
        __fini_priority_array_end = ABSOLUTE(.);

        *(.gnu.linkonce.r.*)
        *(.rodata)
        *(.rodata*)
        *(.srodata)
        *(.srodata*)
        *(.gnu.linkonce.e.*)
    } > ram

    .data ALIGN(4):
    {
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
    } > ram

    .bss ALIGN(4) :
    {
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
    } > ram

    .stack ALIGN(16) :
    {
        *(.stack)
        *(.stack*)
        PROVIDE(end = .);
    } > ram

    __stack_top = ALIGN_DOWN(ORIGIN(ram) + LENGTH(ram), 16);

    . = ORIGIN(shared_mem_ram);
    .shared_mem (ALIGN(4)) :
    {
        KEEP(*(.shared_mem))
    } > shared_mem_ram
}
