#ifndef K_ATOMIC_H
#define K_ATOMIC_H
#include "stdtypes.h"

/*
	kernel critical code enter and exit
	
	provides per CPU locking, where a CPU can require the lock
	infinite number of times and release once
*/
#define KCCENTER(ptr) katomic_ccenter_wfe(ptr)
#define KCCEXIT(ptr) katomic_ccexit_sev(ptr)

typedef struct _KATOMIC_CCLOCK {
	uint32 volatile			lock;
	uint32 volatile			cnt;
} KATOMIC_CCLOCK;

uint32 katomic_locktake(volatile uint32 *ptr, uint32 need_value, uint32 new_value);
int katomic_lockspin(volatile uint32 *ptr, uint32 id);
void katomic_ccenter(volatile uint32 *ptr);

int katomic_lockspin_wfe(volatile KATOMIC_CCLOCK *ptr, uint32 id);
void katomic_ccexit_sev(volatile KATOMIC_CCLOCK *ptr);
void katomic_ccenter_wfe(volatile KATOMIC_CCLOCK *ptr);
#endif