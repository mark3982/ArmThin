#ifndef CORELIB_CORE_H
#define CORELIB_CORE_H
#include "stdtypes.h"
#include "rb.h"

/* kernel communication */
extern ERH				__corelib_rx;
extern ERH				__corelib_tx;

/* signaling */
void notifykserver();
int signal(uintptr proc, uintptr thread, uintptr signal);
void wakeup(uintptr	proc, uintptr thread);
int getsignal(uintptr *process, uintptr *signal);

/* sleeping, yeild, and time */
int sleep(uint32 timeout);
void yield();
uint32 getTicksPerSecond();
uint32 getosticks();
uint32 sleepticks(uint32 timeout);

/* memory management */
uintptr getpagesize();
void vfree(uintptr addr, uintptr cnt);
uintptr valloc(uintptr cnt);
void *malloc(uint32 size);
void free(void *ptr);

/* random */
int rand();
void srand(unsigned int seed);
uint32 __rand(uint32 next);

/* string/memory */
void memset(void *p, uint8 v, uintptr sz);
void memcpy(void *_a, void *_b, uintptr sz);

/* serial debug output */
void printf(const char *fmt, ...);
void sprintf(char *buf, const char *fmt, ...);
char* itoh(int i, char *buf);
#endif