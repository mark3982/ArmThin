#ifndef DBGOUT_H
#define DBGOUT_H
#include "stdtypes.h"

void kserdbg_putc(char c);
void kserdbg_puts(const char * str);
char* itoh(int i, char *buf);
void ksprintf(char *buf, const char *fmt, ...);
void kprintf(const char *fmt, ...);
void stackprinter();
#endif