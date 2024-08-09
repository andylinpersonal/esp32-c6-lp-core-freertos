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
#include <string.h>

ATOMIC(bool) lp_core_started = false;
ATOMIC(bool) hp_core_init    = false;
uint64_t lp_core_mcycle      = 0;

static const char TAG[] = "ulp";

/*-----------------------------------------------------------*/
static StaticSemaphore_t print_lock_buffer;
static SemaphoreHandle_t print_lock = NULL;
static StaticTimer_t     test_timer_buffer;
static StaticTask_t      test_task_tcb;
static const char        test_task_name[] = "lp-test";

static StackType_t __attribute__((aligned(portBYTE_ALIGNMENT), section(".stack"))) test_task_stack[192U];

static void test_timer_func(TimerHandle_t htimer)
{
	portENTER_CRITICAL();
	lp_core_print_hex((int)xTaskGetTickCount());
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
		lp_core_print_hex((int)xTaskGetTickCount());
		lp_core_print_str(" [");
		lp_core_print_str(pcTaskGetName(NULL));
		lp_core_print_str("] greeting from lp core");
		lp_core_print_str("\n");
		portEXIT_CRITICAL();

		for (size_t i = 0; i < WAIT_RATIO; i++) {
			xSemaphoreTake(print_lock, portMAX_DELAY);
		}
		atomic_store(&lp_core_started, 1);
	}
}
/*-----------------------------------------------------------*/

extern int end[];
extern int __stack_top[];

int main(void)
{
	ulp_lp_core_delay_us(10000000);

	lp_core_print_str("stack@0x");
	lp_core_print_hex((int)end);
	lp_core_print_str("-0x");
	lp_core_print_hex((int)__stack_top);
	lp_core_print_str("\n");
	xTaskCreateStatic(test_task_func, test_task_name, sizeof(test_task_stack) / sizeof(StackType_t), NULL, 2,
	                  &(test_task_stack[0]), &(test_task_tcb));

	/* Start the scheduler. */
	vTaskStartScheduler();
	abort();
}
/*-----------------------------------------------------------*/
