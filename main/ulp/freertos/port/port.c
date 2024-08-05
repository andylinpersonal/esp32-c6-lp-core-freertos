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

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the RISC-V port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "portmacro.h"
#include "task.h"

/* Standard includes. */
#include "string.h"

#include <sdkconfig.h>

/* Low-level includes for ESP32-C6 */
#ifdef CONFIG_IDF_TARGET_ESP32C6
#include <hal/clk_tree_ll.h>
#include <hal/lp_timer_ll.h>
#include <riscv/csr.h>
#include <riscv/rv_utils.h>
#include <soc/rtc.h>
#include <ulp_lp_core_interrupts.h>
#endif

/* Let the user override the pre-loading of the initial RA. */
#ifdef configTASK_RETURN_ADDRESS
#define portTASK_RETURN_ADDRESS configTASK_RETURN_ADDRESS
#else
#define portTASK_RETURN_ADDRESS 0
#endif

/* The stack used by interrupt service routines.  Set configISR_STACK_SIZE_WORDS
 * to use a statically allocated array as the interrupt stack.  Alternative leave
 * configISR_STACK_SIZE_WORDS undefined and update the linker script so that a
 * linker variable names __freertos_irq_stack_top has the same value as the top
 * of the stack used by main.  Using the linker script method will repurpose the
 * stack that was used by main before the scheduler was started for use as the
 * interrupt stack after the scheduler has started. */
#ifdef configISR_STACK_SIZE_WORDS
static __attribute__((aligned(16), section(".stack"))) StackType_t xISRStack[configISR_STACK_SIZE_WORDS] = {0};
const StackType_t xISRStackTop = (StackType_t) & (xISRStack[configISR_STACK_SIZE_WORDS & ~portBYTE_ALIGNMENT_MASK]);

/* Don't use 0xa5 as the stack fill bytes as that is used by the kernel for
 * the task stacks, and so will legitimately appear in many positions within
 * the ISR stack. */
#define portISR_STACK_FILL_BYTE 0xee
#else
/* Repurpose the default stack as ISR stack. */
extern const uint32_t __stack_top[];
const StackType_t     xISRStackTop = (StackType_t)&__stack_top;
#endif

/*
 * Setup the timer to generate the tick interrupts.  The implementation in this
 * file is weak to allow application writers to change the timer used to
 * generate the tick interrupt.
 */
void vPortSetupTimerInterrupt(void) __attribute__((weak));

/*-----------------------------------------------------------*/
/* Heap storage */
uint8_t ucHeap[configTOTAL_HEAP_SIZE];
/* Used to program the machine timer compare register. */
uint64_t        ullNextTime  = 0ULL;
const uint64_t *pullNextTime = &ullNextTime;
size_t          uxTimerIncrementsForOneTick =
    (size_t)((configCPU_CLOCK_HZ) / (configTICK_RATE_HZ)); /* Assumes increment won't go over 32-bits. */

/* Holds the critical nesting value - deliberately non-zero at start up to
 * ensure interrupts are not accidentally enabled before the scheduler starts. */
size_t  xCriticalNesting  = (size_t)0xaaaaaaaa;
size_t *pxCriticalNesting = &xCriticalNesting;

/* Used to catch tasks that attempt to return from their implementing function. */
size_t xTaskReturnAddress = (size_t)portTASK_RETURN_ADDRESS;

/* Set configCHECK_FOR_STACK_OVERFLOW to 3 to add ISR stack checking to task
 * stack checking.  A problem in the ISR stack will trigger an assert, not call
 * the stack overflow hook function (because the stack overflow hook is specific
 * to a task stack, not the ISR stack). */
#if defined(configISR_STACK_SIZE_WORDS) && (configCHECK_FOR_STACK_OVERFLOW > 2)
#warning "This path not tested, or even compiled yet."

static const uint8_t ucExpectedStackBytes[] = {
    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,
    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,
    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,
    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE,
    portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE, portISR_STACK_FILL_BYTE};

#define portCHECK_ISR_STACK() \
	configASSERT((memcmp((void *)xISRStack, (void *)ucExpectedStackBytes, sizeof(ucExpectedStackBytes)) == 0))
#else /* if defined( configISR_STACK_SIZE_WORDS ) && ( configCHECK_FOR_STACK_OVERFLOW > 2 ) */
/* Define the function away. */
#define portCHECK_ISR_STACK()
#endif /* configCHECK_FOR_STACK_OVERFLOW > 2 */

/*-----------------------------------------------------------*/

#ifdef CONFIG_IDF_TARGET_ESP32C6
/* Enable interrupt 30, which all external interrupts are routed to*/
#define MIE_ALL_INTS_MASK (1 << 30)

void prvSysTickReloadTimer()
{
	// ulp_lp_core_lp_timer_get_cycle_count
	lp_timer_ll_counter_snapshot(&LP_TIMER);

	uint32_t lo = lp_timer_ll_get_counter_value_low(&LP_TIMER, 0);
	uint32_t hi = lp_timer_ll_get_counter_value_high(&LP_TIMER, 0);

	lp_timer_counter_value_t result = {.lo = lo, .hi = hi};

#define TIMER_ID 1
	// ulp_lp_core_lp_timer_set_wakeup_time
	uint64_t cycle_cnt = result.val;
	ullNextTime        = cycle_cnt + uxTimerIncrementsForOneTick;
	lp_timer_ll_clear_lp_alarm_intr_status(&LP_TIMER);
	lp_timer_ll_set_alarm_target(&LP_TIMER, TIMER_ID, ullNextTime);
	lp_timer_ll_set_target_enable(&LP_TIMER, TIMER_ID, true);
}

void vSysTickISRHandler(void)
{
	prvSysTickReloadTimer();
	if (xTaskIncrementTick() == pdTRUE) {
		vTaskSwitchContext();
	}
}

inline static uint32_t uxSysTickGetTimerIntervalForOneTick(uint64_t duration_us)
{
	return (uint32_t)(duration_us * (1 << RTC_CLK_CAL_FRACT) / clk_ll_rtc_slow_load_cal());
}

void vPortSetupTimerInterrupt(void)
{
	uxTimerIncrementsForOneTick = uxSysTickGetTimerIntervalForOneTick(1000000U / configTICK_RATE_HZ);
	prvSysTickReloadTimer();

	RV_SET_CSR(mie, MIE_ALL_INTS_MASK);
	LP_TIMER.lp_int_en.alarm = 1;
}
#endif /* ( configMTIME_BASE_ADDRESS != 0 ) && ( configMTIME_BASE_ADDRESS != 0 ) */
/*-----------------------------------------------------------*/

BaseType_t xPortStartScheduler(void)
{
	extern void xPortStartFirstTask(void);

#if (configASSERT_DEFINED == 1)
	{
		/* Check alignment of the interrupt stack - which is the same as the
		 * stack that was being used by main() prior to the scheduler being
		 * started. */
		configASSERT((xISRStackTop & portBYTE_ALIGNMENT_MASK) == 0);

#ifdef configISR_STACK_SIZE_WORDS
		{
			memset((void *)xISRStack, portISR_STACK_FILL_BYTE, sizeof(xISRStack));
		}
#endif /* configISR_STACK_SIZE_WORDS */
	}
#endif /* configASSERT_DEFINED */

	/* If there is a CLINT then it is ok to use the default implementation
	 * in this file, otherwise vPortSetupTimerInterrupt() must be implemented to
	 * configure whichever clock is to be used to generate the tick interrupt. */
	vPortSetupTimerInterrupt();

	xPortStartFirstTask();

	/* Should not get here as after calling xPortStartFirstTask() only tasks
	 * should be executing. */
	return pdFAIL;
}

/*-----------------------------------------------------------*/

void vPortEndScheduler(void)
{
	/* Not implemented. */
	for (;;) {
	}
}

/*-----------------------------------------------------------*/

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer,
                                    configSTACK_DEPTH_TYPE *puxTimerTaskStackSize)
{
	static StaticTask_t                                   xTimerTaskTCB;
	static __attribute__((aligned(portBYTE_ALIGNMENT),
	                      section(".stack"))) StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];

	*ppxTimerTaskTCBBuffer   = &(xTimerTaskTCB);
	*ppxTimerTaskStackBuffer = &(uxTimerTaskStack[0]);
	*puxTimerTaskStackSize   = configTIMER_TASK_STACK_DEPTH;
}

/*-----------------------------------------------------------*/

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer,
                                   configSTACK_DEPTH_TYPE *puxIdleTaskStackSize)
{
	static StaticTask_t                                   xIdleTaskTCB;
	static __attribute__((aligned(portBYTE_ALIGNMENT),
	                      section(".stack"))) StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

	*ppxIdleTaskTCBBuffer   = &(xIdleTaskTCB);
	*ppxIdleTaskStackBuffer = &(uxIdleTaskStack[0]);
	*puxIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

#if (configCHECK_FOR_STACK_OVERFLOW > 0)

TaskHandle_t g_overflowed_task      = NULL;
const char  *g_overflowed_task_name = NULL;

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	g_overflowed_task      = xTask;
	g_overflowed_task_name = pcTaskName;
	abort();
}

#endif /* #if ( configCHECK_FOR_STACK_OVERFLOW > 0 ) */
/*-----------------------------------------------------------*/
