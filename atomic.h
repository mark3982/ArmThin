#ifndef K_ATOMIC_H
#define K_ATOMIC_H
#include "stdtypes.h"

uint32 katomic_exchange(volatile uint32* ptr, uint32 new_value);
#endif