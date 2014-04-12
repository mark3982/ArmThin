#ifndef CORELIB_CORE_H
#define CORELIB_CORE_H
#include "../stdtypes.h"
#include "rb.h"

extern RBM				__corelib_rx;
extern RBM				__corelib_tx;


int sleep(uint32 timeout);
void yield();
uint32 getTicksPerSecond();
int rand();
void srand(unsigned int seed);

void printf(const char *fmt, ...);
void sprintf(char *buf, const char *fmt, ...);
char* itoh(int i, char *buf);
#endif