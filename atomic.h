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

/* 8-bit lock for normal and kernel threads (seperate version) wfe=kernel yield=thread */
uint32 katomic_locktake8(volatile uint8 *ptr, uint8 need_value, uint8 new_value);
int katomic_lockspin_yield8nr(volatile uint8 *ptr, uint8 id);
int katomic_lockspin_wfe8nr(volatile uint8 *ptr, uint8 id);

/* for normal threads */
void katomic_ccenter_yield(volatile KATOMIC_CCLOCK *ptr);
void katomic_ccexit_yield(volatile KATOMIC_CCLOCK *ptr);

/* for CPUs */
int katomic_lockspin_wfe(volatile KATOMIC_CCLOCK *ptr, uint32 id);
void katomic_ccexit_sev(volatile KATOMIC_CCLOCK *ptr);
void katomic_ccenter_wfe(volatile KATOMIC_CCLOCK *ptr);
#endif