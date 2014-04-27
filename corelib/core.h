#ifndef CORELIB_CORE_H
#define CORELIB_CORE_H
#include "stdtypes.h"
#include "rb.h"

extern ERH				__corelib_rx;
extern ERH				__corelib_tx;

void notifykserver();
int sleep(uint32 timeout);
void yield();
uint32 getTicksPerSecond();
int signal(uintptr proc, uintptr thread, uintptr signal);
void wakeup(uintptr	proc, uintptr thread);
uintptr getsignal();

void vfree(uintptr addr, uintptr cnt);
uintptr valloc(uintptr cnt);

int rand();
void srand(unsigned int seed);
uint32 __rand(uint32 next);

void printf(const char *fmt, ...);
void sprintf(char *buf, const char *fmt, ...);
char* itoh(int i, char *buf);
#endif