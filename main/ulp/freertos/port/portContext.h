/*
 * FreeRTOS Kernel V11.1.0
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * https://www.FreeRTOS.org
 * https://github.com/FreeRTOS
 *
 */

#ifndef PORTCONTEXT_H
#define PORTCONTEXT_H

.macro portasmSAVE_ADDITIONAL_REGISTERS
/* No additional registers to save, so this macro does nothing. */
.endm

.macro portasmRESTORE_ADDITIONAL_REGISTERS
/* No additional registers to restore, so this macro does nothing. */
.endm

#define portasmADDITIONAL_CONTEXT_SIZE 0

#if __riscv_xlen == 32
#define store_x          sw
#define load_x           lw
#define portWORD_SIZE    4
#else
#error "Unsupported XLEN"
#endif

/* Only the standard core registers are stored by default.  Any additional
 * registers must be saved by the portasmSAVE_ADDITIONAL_REGISTERS and
 * portasmRESTORE_ADDITIONAL_REGISTERS macros - which can be defined in a chip
 * specific version of freertos_risc_v_chip_specific_extensions.h.  See the
 * notes at the top of portASM.S file. */
#define portCONTEXT_SIZE               ( 33 * portWORD_SIZE )
#define portCRITICAL_NESTING_OFFSET    31
#define portMSTATUS_OFFSET             32

/*-----------------------------------------------------------*/

.extern pxCurrentTCB
.extern xISRStackTop
.extern xCriticalNesting
.extern pxCriticalNesting
/*-----------------------------------------------------------*/

.macro portcontextSAVE_CONTEXT_INTERNAL
addi sp, sp, -portCONTEXT_SIZE
store_x x1,  1  * portWORD_SIZE(sp)
store_x x3,  2  * portWORD_SIZE(sp)
store_x x4,  3  * portWORD_SIZE(sp)
store_x x5,  4  * portWORD_SIZE(sp)
store_x x6,  5  * portWORD_SIZE(sp)
store_x x7,  6  * portWORD_SIZE(sp)
store_x x8,  7  * portWORD_SIZE(sp)
store_x x9,  8  * portWORD_SIZE(sp)
store_x x10, 9  * portWORD_SIZE(sp)
store_x x11, 10 * portWORD_SIZE(sp)
store_x x12, 11 * portWORD_SIZE(sp)
store_x x13, 12 * portWORD_SIZE(sp)
store_x x14, 13 * portWORD_SIZE(sp)
store_x x15, 14 * portWORD_SIZE(sp)
store_x x16, 15 * portWORD_SIZE(sp)
store_x x17, 16 * portWORD_SIZE(sp)
store_x x18, 17 * portWORD_SIZE(sp)
store_x x19, 18 * portWORD_SIZE(sp)
store_x x20, 19 * portWORD_SIZE(sp)
store_x x21, 20 * portWORD_SIZE(sp)
store_x x22, 21 * portWORD_SIZE(sp)
store_x x23, 22 * portWORD_SIZE(sp)
store_x x24, 23 * portWORD_SIZE(sp)
store_x x25, 24 * portWORD_SIZE(sp)
store_x x26, 25 * portWORD_SIZE(sp)
store_x x27, 26 * portWORD_SIZE(sp)
store_x x28, 27 * portWORD_SIZE(sp)
store_x x29, 28 * portWORD_SIZE(sp)
store_x x30, 29 * portWORD_SIZE(sp)
store_x x31, 30 * portWORD_SIZE(sp)

load_x t0, xCriticalNesting                                   /* Load the value of xCriticalNesting into t0. */
store_x t0, portCRITICAL_NESTING_OFFSET * portWORD_SIZE(sp)   /* Store the critical nesting value to the stack. */

csrr t0, mstatus /* Required for MPIE bit. */
store_x t0, portMSTATUS_OFFSET * portWORD_SIZE(sp)

portasmSAVE_ADDITIONAL_REGISTERS /* Defined in freertos_risc_v_chip_specific_extensions.h to save any registers unique to the RISC-V implementation. */

load_x t0, pxCurrentTCB          /* Load pxCurrentTCB. */
store_x sp, 0(t0)                /* Write sp to first TCB member. */

.endm
/*-----------------------------------------------------------*/

.macro portcontextSAVE_EXCEPTION_CONTEXT
portcontextSAVE_CONTEXT_INTERNAL
csrr a0, mcause
csrr a1, mepc
addi a1, a1, 4          /* Synchronous so update exception return address to the instruction after the instruction that generated the exception. */
store_x a1, 0(sp)       /* Save updated exception return address. */
load_x sp, xISRStackTop /* Switch to ISR stack. */
.endm
/*-----------------------------------------------------------*/

.macro portcontextSAVE_INTERRUPT_CONTEXT
portcontextSAVE_CONTEXT_INTERNAL
csrr a0, mcause
csrr a1, mepc
store_x a1, 0(sp)       /* Asynchronous interrupt so save unmodified exception return address. */
load_x sp, xISRStackTop /* Switch to ISR stack. */
.endm
/*-----------------------------------------------------------*/

.macro portcontextRESTORE_CONTEXT
load_x t1, pxCurrentTCB /* Load pxCurrentTCB. */
load_x sp, 0(t1)        /* Read sp from first TCB member. */

/* Load mepc with the address of the instruction in the task to run next. */
load_x t0, 0(sp)
csrw mepc, t0

/* Defined in freertos_risc_v_chip_specific_extensions.h to restore any registers unique to the RISC-V implementation. */
portasmRESTORE_ADDITIONAL_REGISTERS

/* Load mstatus with the interrupt enable bits used by the task. */
load_x t0, portMSTATUS_OFFSET * portWORD_SIZE(sp)
csrw mstatus, t0                                             /* Required for MPIE bit. */

load_x t0, portCRITICAL_NESTING_OFFSET * portWORD_SIZE(sp)   /* Obtain xCriticalNesting value for this task from task's stack. */
load_x t1, pxCriticalNesting                                 /* Load the address of xCriticalNesting into t1. */
store_x t0, 0(t1)                                            /* Restore the critical nesting value for this task. */

load_x x1,  1  * portWORD_SIZE(sp)
load_x x3,  2  * portWORD_SIZE(sp)
load_x x4,  3  * portWORD_SIZE(sp)
load_x x5,  4  * portWORD_SIZE(sp)
load_x x6,  5  * portWORD_SIZE(sp)
load_x x7,  6  * portWORD_SIZE(sp)
load_x x8,  7  * portWORD_SIZE(sp)
load_x x9,  8  * portWORD_SIZE(sp)
load_x x10, 9  * portWORD_SIZE(sp)
load_x x11, 10 * portWORD_SIZE(sp)
load_x x12, 11 * portWORD_SIZE(sp)
load_x x13, 12 * portWORD_SIZE(sp)
load_x x14, 13 * portWORD_SIZE(sp)
load_x x15, 14 * portWORD_SIZE(sp)
load_x x16, 15 * portWORD_SIZE(sp)
load_x x17, 16 * portWORD_SIZE(sp)
load_x x18, 17 * portWORD_SIZE(sp)
load_x x19, 18 * portWORD_SIZE(sp)
load_x x20, 19 * portWORD_SIZE(sp)
load_x x21, 20 * portWORD_SIZE(sp)
load_x x22, 21 * portWORD_SIZE(sp)
load_x x23, 22 * portWORD_SIZE(sp)
load_x x24, 23 * portWORD_SIZE(sp)
load_x x25, 24 * portWORD_SIZE(sp)
load_x x26, 25 * portWORD_SIZE(sp)
load_x x27, 26 * portWORD_SIZE(sp)
load_x x28, 27 * portWORD_SIZE(sp)
load_x x29, 28 * portWORD_SIZE(sp)
load_x x30, 29 * portWORD_SIZE(sp)
load_x x31, 30 * portWORD_SIZE(sp)
addi sp, sp, portCONTEXT_SIZE

.endm
/*-----------------------------------------------------------*/

#endif /* PORTCONTEXT_H */
