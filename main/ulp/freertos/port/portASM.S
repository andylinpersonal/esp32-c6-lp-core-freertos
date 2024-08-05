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

/*
 * The FreeRTOS kernel's RISC-V port is split between the the code that is
 * common across all currently supported RISC-V chips (implementations of the
 * RISC-V ISA), and code which tailors the port to a specific RISC-V chip:
 *
 * + The code that is common to all RISC-V chips is implemented in
 *   FreeRTOS\Source\portable\GCC\RISC-V-RV32\portASM.S.  There is only one
 *   portASM.S file because the same file is used no matter which RISC-V chip is
 *   in use.
 *
 * + The code that tailors the kernel's RISC-V port to a specific RISC-V
 *   chip is implemented in freertos_risc_v_chip_specific_extensions.h.  There
 *   is one freertos_risc_v_chip_specific_extensions.h that can be used with any
 *   RISC-V chip that both includes a standard CLINT and does not add to the
 *   base set of RISC-V registers.  There are additional
 *   freertos_risc_v_chip_specific_extensions.h files for RISC-V implementations
 *   that do not include a standard CLINT or do add to the base set of RISC-V
 *   registers.
 *
 * CARE MUST BE TAKEN TO INCLUDE THE CORRECT
 * freertos_risc_v_chip_specific_extensions.h HEADER FILE FOR THE CHIP
 * IN USE.  To include the correct freertos_risc_v_chip_specific_extensions.h
 * header file ensure the path to the correct header file is in the assembler's
 * include path.
 *
 * This freertos_risc_v_chip_specific_extensions.h is for use on RISC-V chips
 * that include a standard CLINT and do not add to the base set of RISC-V
 * registers.
 *
 */

#include "portContext.h"
#include "FreeRTOSConfig.h"

#include <soc/soc_caps.h>

.global xPortStartFirstTask
.global pxPortInitialiseStack
.global _panic_handler
.global _interrupt_handler

.extern vTaskSwitchContext
.extern xTaskIncrementTick
.extern xTaskReturnAddress

#if configCHECK_FOR_STACK_OVERFLOW > 0
.extern g_overflowed_task
.extern g_overflowed_task_name
.extern lp_core_print_str
.extern lp_core_print_hex
#endif

.extern _panic_handler__real
.extern ulp_lp_core_intr_handler
/*-----------------------------------------------------------*/

/*
 * Unlike other ports pxPortInitialiseStack() is written in assembly code as it
 * needs access to the portasmADDITIONAL_CONTEXT_SIZE constant.  The prototype
 * for the function is as per the other ports:
 * StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack, TaskFunction_t pxCode, void *pvParameters );
 *
 * As per the standard RISC-V ABI pxTopOfStack is passed in in a0, pxCode in
 * a1, and pvParameters in a2.  The new top of stack is passed out in a0.
 *
 * RISC-V maps registers to ABI names as follows (X1 to X31 integer registers
 * for the 'I' profile, X1 to X15 for the 'E' profile, currently I assumed).
 *
 * Register      ABI Name    Description                       Saver
 * x0            zero        Hard-wired zero                   -
 * x1            ra          Return address                    Caller
 * x2            sp          Stack pointer                     Callee
 * x3            gp          Global pointer                    -
 * x4            tp          Thread pointer                    -
 * x5-7          t0-2        Temporaries                       Caller
 * x8            s0/fp       Saved register/Frame pointer      Callee
 * x9            s1          Saved register                    Callee
 * x10-11        a0-1        Function Arguments/return values  Caller
 * x12-17        a2-7        Function arguments                Caller
 * x18-27        s2-11       Saved registers                   Callee
 * x28-31        t3-6        Temporaries                       Caller
 *
 * The RISC-V context is saved to FreeRTOS tasks in the following stack frame,
 * where the global and thread pointers are currently assumed to be constant so
 * are not saved:
 *
 * mstatus
 * xCriticalNesting
 * x31
 * x30
 * x29
 * x28
 * x27
 * x26
 * x25
 * x24
 * x23
 * x22
 * x21
 * x20
 * x19
 * x18
 * x17
 * x16
 * x15
 * x14
 * x13
 * x12
 * x11
 * pvParameters
 * x9
 * x8
 * x7
 * x6
 * x5
 * portTASK_RETURN_ADDRESS
 * [chip specific registers go here]
 * pxCode
 */
.type pxPortInitialiseStack,@function
pxPortInitialiseStack:
    csrr t0, mstatus                    /* Obtain current mstatus value. */
    andi t0, t0, ~0x8                   /* Ensure interrupts are disabled when the stack is restored within an ISR.  Required when a task is created after the schedulre has been started, otherwise interrupts would be disabled anyway. */
    addi t1, x0, 0x188                  /* Generate the value 0x1880, which are the MPIE and MPP bits to set in mstatus. */
    slli t1, t1, 4
    or t0, t0, t1                       /* Set MPIE and MPP bits in mstatus value. */

    addi a0, a0, -portWORD_SIZE
    store_x t0, 0(a0)                   /* mstatus onto the stack. */
    addi a0, a0, -portWORD_SIZE         /* Space for critical nesting count. */
    store_x x0, 0(a0)                   /* Critical nesting count starts at 0 for every task. */

    addi a0, a0, -(22 * portWORD_SIZE)  /* Space for registers x10-x31. */

    store_x a2, 0(a0)                   /* Task parameters (pvParameters parameter) goes into register X10/a0 on the stack. */
    addi a0, a0, -(6 * portWORD_SIZE)   /* Space for registers x5-x9 + taskReturnAddress. */
    load_x t0, xTaskReturnAddress
    store_x t0, 0(a0)                   /* Return address onto the stack. */
    addi t0, x0, portasmADDITIONAL_CONTEXT_SIZE /* The number of chip specific additional registers. */
chip_specific_stack_frame:              /* First add any chip specific registers to the stack frame being created. */
    beq t0, x0, 1f                      /* No more chip specific registers to save. */
    addi a0, a0, -portWORD_SIZE         /* Make space for chip specific register. */
    store_x x0, 0(a0)                   /* Give the chip specific register an initial value of zero. */
    addi t0, t0, -1                     /* Decrement the count of chip specific registers remaining. */
    j chip_specific_stack_frame         /* Until no more chip specific registers. */
1:
    addi a0, a0, -portWORD_SIZE
    store_x a1, 0(a0)                   /* mret value (pxCode parameter) onto the stack. */
    ret
/*-----------------------------------------------------------*/

.type xPortStartFirstTask,@function
xPortStartFirstTask:
    load_x  sp, pxCurrentTCB            /* Load pxCurrentTCB. */
    load_x  sp, 0( sp )                 /* Read sp from first TCB member. */

    load_x  x1, 0( sp ) /* Note for starting the scheduler the exception return address is used as the function return address. */

    portasmRESTORE_ADDITIONAL_REGISTERS /* Defined in freertos_risc_v_chip_specific_extensions.h to restore any registers unique to the RISC-V implementation. */

    load_x  x7, 4 * portWORD_SIZE( sp )     /* t2 */
    load_x  x8, 5 * portWORD_SIZE( sp )     /* s0/fp */
    load_x  x9, 6 * portWORD_SIZE( sp )     /* s1 */
    load_x  x10, 7 * portWORD_SIZE( sp )    /* a0 */
    load_x  x11, 8 * portWORD_SIZE( sp )    /* a1 */
    load_x  x12, 9 * portWORD_SIZE( sp )    /* a2 */
    load_x  x13, 10 * portWORD_SIZE( sp )   /* a3 */
    load_x  x14, 11 * portWORD_SIZE( sp )   /* a4 */
    load_x  x15, 12 * portWORD_SIZE( sp )   /* a5 */
    load_x  x16, 13 * portWORD_SIZE( sp )   /* a6 */
    load_x  x17, 14 * portWORD_SIZE( sp )   /* a7 */
    load_x  x18, 15 * portWORD_SIZE( sp )   /* s2 */
    load_x  x19, 16 * portWORD_SIZE( sp )   /* s3 */
    load_x  x20, 17 * portWORD_SIZE( sp )   /* s4 */
    load_x  x21, 18 * portWORD_SIZE( sp )   /* s5 */
    load_x  x22, 19 * portWORD_SIZE( sp )   /* s6 */
    load_x  x23, 20 * portWORD_SIZE( sp )   /* s7 */
    load_x  x24, 21 * portWORD_SIZE( sp )   /* s8 */
    load_x  x25, 22 * portWORD_SIZE( sp )   /* s9 */
    load_x  x26, 23 * portWORD_SIZE( sp )   /* s10 */
    load_x  x27, 24 * portWORD_SIZE( sp )   /* s11 */
    load_x  x28, 25 * portWORD_SIZE( sp )   /* t3 */
    load_x  x29, 26 * portWORD_SIZE( sp )   /* t4 */
    load_x  x30, 27 * portWORD_SIZE( sp )   /* t5 */
    load_x  x31, 28 * portWORD_SIZE( sp )   /* t6 */

    load_x  x5, portCRITICAL_NESTING_OFFSET * portWORD_SIZE( sp )    /* Obtain xCriticalNesting value for this task from task's stack. */
    load_x  x6, pxCriticalNesting           /* Load the address of xCriticalNesting into x6. */
    store_x x5, 0( x6 )                     /* Restore the critical nesting value for this task. */

    load_x  x5, portMSTATUS_OFFSET * portWORD_SIZE( sp )    /* Initial mstatus into x5 (t0). */
    addi    x5, x5, 0x08                    /* Set MIE bit so the first task starts with interrupts enabled - required as returns with ret not eret. */
    csrrw   x0, mstatus, x5                 /* Interrupts enabled from here! */

    load_x  x5, 2 * portWORD_SIZE( sp )     /* Initial x5 (t0) value. */
    load_x  x6, 3 * portWORD_SIZE( sp )     /* Initial x6 (t1) value. */

    addi    sp, sp, portCONTEXT_SIZE
    ret
/*-----------------------------------------------------------*/

.section .rodata.handlers,"a"
.type task_prefix,@object
task_prefix:
    .asciz "Task: "

.type task_name_placeholder,@object
task_name_placeholder:
    .asciz "UNK"

.section .text.handlers,"ax"
.type _panic_handler,@function
_panic_handler:
    portcontextSAVE_EXCEPTION_CONTEXT
    /* a0 now contains mcause. */
    li  t0, 11                           /* 11 == environment call. */
    bne a0, t0, other_exception         /* Not an M environment call, so some other exception. */
    call vTaskSwitchContext
    portcontextRESTORE_CONTEXT
    mret

other_exception:
#if 0
// #if configCHECK_FOR_STACK_OVERFLOW > 0
    lw   a0, task_prefix
    call lp_core_print_str
    lw   a0, g_overflowed_task_name
    bnez a0, print_task_name
    lw   a0, task_name_placeholder
print_task_name:
    call lp_core_print_str
    lw   a0, g_overflowed_task
    call lp_core_print_hex
#endif // configCHECK_FOR_STACK_OVERFLOW > 0
    portcontextRESTORE_CONTEXT
    j    _panic_handler__real
/*-----------------------------------------------------------*/

#if SOC_LP_CORE_SINGLE_INTERRUPT_VECTOR

.section .text.handlers,"ax"
.type _interrupt_handler, @function
_interrupt_handler:
    portcontextSAVE_INTERRUPT_CONTEXT
    call ulp_lp_core_intr_handler
    portcontextRESTORE_CONTEXT
    mret

#endif // SOC_LP_CORE_SINGLE_INTERRUPT_VECTOR
