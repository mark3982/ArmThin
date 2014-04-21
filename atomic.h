#ifndef K_ATOMIC_H
#define K_ATOMIC_H
#include "stdtypes.h"

#define KCCENTER(ptr) katomic_ccenter(ptr)
#define KCCEXIT(ptr) katomic_ccexit(ptr)

uint32 katomic_locktake(volatile uint32 *ptr, uint32 need_value, uint32 new_value);
int katomic_lockspin(volatile uint32 *ptr, uint32 id);
void katomic_ccenter(volatile uint32 *ptr);
void katomic_ccexit(volatile uint32 *ptr);
#endif