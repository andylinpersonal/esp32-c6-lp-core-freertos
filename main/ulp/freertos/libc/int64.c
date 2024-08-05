
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
