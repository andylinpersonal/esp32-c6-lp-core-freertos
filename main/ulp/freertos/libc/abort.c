void __attribute__((noreturn)) abort(void)
{
	__asm__("ebreak\n\t");
	while (1) {
	}
}
