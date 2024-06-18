#include "lp_common.h"

#include <riscv/rv_utils.h>
#include <ulp_lp_core_print.h>
#include <ulp_lp_core_utils.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include <queue.h>
#include <semphr.h>
#include <task.h>
#include <timers.h>

/* Standard includes. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

ATOMIC(uint32_t) lp_core_started = 0;
uint64_t lp_core_mcycle = 0;

static const char TAG[] = "ulp";

/*-----------------------------------------------------------*/
static StaticSemaphore_t print_lock_buffer;
static SemaphoreHandle_t print_lock = NULL;
static StaticTimer_t test_timer_buffer;
static StaticTask_t test_task_tcb;
static StackType_t __attribute__((section(".stack"))) test_task_stack[168U];
static const char test_task_name[] = "lp-test";

static void test_timer_func(TimerHandle_t htimer)
{
	portENTER_CRITICAL();
	lp_core_print_hex(xTaskGetTickCount());
	lp_core_print_str(" [");
	lp_core_print_str(pcTaskGetName(NULL));
	lp_core_print_str("] ");
	lp_core_print_str(pcTimerGetName(htimer));
	lp_core_print_str("\n");
	portEXIT_CRITICAL();
	xSemaphoreGive(print_lock);
}

/*-----------------------------------------------------------*/

#define WAIT_RATIO 3

static void test_task_func(void *)
{
	print_lock = xSemaphoreCreateCountingStatic(WAIT_RATIO, 0, &print_lock_buffer);

	TimerHandle_t h_timer =
	    xTimerCreateStatic("timer", pdMS_TO_TICKS(1000), pdTRUE, NULL, &test_timer_func, &test_timer_buffer);

	xTimerStart(h_timer, portMAX_DELAY);

	for (;;) {
		portENTER_CRITICAL();
		lp_core_print_hex(xTaskGetTickCount());
		lp_core_print_str(" [");
		lp_core_print_str(pcTaskGetName(NULL));
		lp_core_print_str("] greeting from lp core");
		lp_core_print_str("\n");
		portEXIT_CRITICAL();

		for (size_t i = 0; i < WAIT_RATIO; i++) {
			xSemaphoreTake(print_lock, portMAX_DELAY);
		}

		lp_core_started++;
	}
}
/*-----------------------------------------------------------*/

extern uint32_t end[];
extern uint32_t __stack_top[];

int main(void)
{
	lp_core_print_str("stack@0x");
	lp_core_print_hex(&end);
	lp_core_print_str("-0x");
	lp_core_print_hex(&__stack_top);
	lp_core_print_str("\n");
	xTaskCreateStatic(test_task_func, test_task_name, sizeof(test_task_stack) / sizeof(StackType_t), NULL, 2,
	                  &(test_task_stack[0]), &(test_task_tcb));

	/* Start the scheduler. */
	vTaskStartScheduler();
	abort();
}
/*-----------------------------------------------------------*/

/** @see https://github.com/t-crest/ospat/blob/master/kernel/libc/__udivdi3.c */
unsigned long long __udivdi3(unsigned long long num, unsigned long long den)
{
	unsigned long long quot, qbit;

	quot = 0;
	qbit = 1;

	if (den == 0) {
		return 0;
	}

	while ((long long)den >= 0) {
		den <<= 1;
		qbit <<= 1;
	}

	while (qbit) {
		if (den <= num) {
			num -= den;
			quot += qbit;
		}
		den >>= 1;
		qbit >>= 1;
	}

	return quot;
}

#if (configCHECK_FOR_STACK_OVERFLOW > 0)

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
	abort();
}

#endif /* #if ( configCHECK_FOR_STACK_OVERFLOW > 0 ) */
/*-----------------------------------------------------------*/
