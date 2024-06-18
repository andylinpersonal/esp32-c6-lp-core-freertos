#pragma once

#include <sdkconfig.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
#include <atomic>
#define ATOMIC(type) std::atomic<type>
#else
#define ATOMIC(type) _Atomic type
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	ATOMIC(char) value;
} lp_spinlock;

#define lp_spinlock_init() {.value = 0}

static inline void lp_spinlock_lock(lp_spinlock *lock)
{
	while (__atomic_test_and_set(&lock->value, __ATOMIC_SEQ_CST)) {
	}
}

static inline void lp_spinlock_unlock(lp_spinlock *lock)
{
	__atomic_clear(&lock->value, __ATOMIC_SEQ_CST);
}

#ifdef __cplusplus
}
#endif
