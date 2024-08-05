#include <sdkconfig.h>

#include <hal/lp_core_ll.h>
#include <soc/soc_caps.h>
#include <stdint.h>

extern void ulp_lp_core_lp_io_intr_handler();
extern void ulp_lp_core_lp_i2c_intr_handler();
extern void ulp_lp_core_lp_uart_intr_handler();
extern void ulp_lp_core_lp_timer_intr_handler();
extern void ulp_lp_core_lp_pmu_intr_handler();

void ulp_lp_core_lp_timer_intr_handler() {}

void ulp_lp_core_lp_timer_intr_handler_wrapper()
{
	extern void vSysTickISRHandler(void);
	vSysTickISRHandler();
	ulp_lp_core_lp_timer_intr_handler();
}

#if SOC_LP_CORE_SINGLE_INTERRUPT_VECTOR

static void *s_intr_handlers[] = {
    ulp_lp_core_lp_io_intr_handler,
    ulp_lp_core_lp_i2c_intr_handler,
    0, // ulp_lp_core_lp_uart_intr_handler TODO: Ignored under FreeRTOS due to spurious interrupt...
    0, // Processed before other interrupts
    0, // Reserved / Unused
    ulp_lp_core_lp_pmu_intr_handler,
};

void ulp_lp_core_intr_handler(void)
{
	uint8_t intr_source = lp_core_ll_get_triggered_interrupt_srcs();

	if (intr_source & (1 << 3)) {
		ulp_lp_core_lp_timer_intr_handler_wrapper();
	}

	for (int i = 0; i < sizeof(s_intr_handlers) / sizeof(void *); i++) {
		if (intr_source & (1 << i)) {
			void (*handler)(void) = s_intr_handlers[i];
			if (handler) {
				handler();
			}
		}
	}
}
#endif
