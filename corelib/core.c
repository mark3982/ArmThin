#include "core.h"
#include "rb.h"

#define SERIAL_BASE 0xa0000000
#define SERIAL_FLAG_REGISTER 0x18
#define SERIAL_BUFFER_FULL (1 << 5)

static unsigned long int next = 1;
 
int rand()
{
    next = next * 1103515245 + 12345;
    return (unsigned int)(next / 65536) % 32768;
}

void srand(unsigned int seed)
{
    next = seed;
}

static void kserdbg_putc(char c) {
    while (*(volatile unsigned long*)(SERIAL_BASE + SERIAL_FLAG_REGISTER) & (SERIAL_BUFFER_FULL));
	
    *(volatile unsigned long*)SERIAL_BASE = c;
}

static void kserdbg_puts(const char * str) {
    while (*str != 0) {
		kserdbg_putc(*str++);
	}
}

char* itoh(int i, char *buf)
{
	const char 	*itoh_map = "0123456789ABCDEF";
	int			n;
	int			b;
	int			z;
	int			s;
	
	if (sizeof(void*) == 4)
		s = 8;
	if (sizeof(void*) == 8)
		s = 16;
	
	for (z = 0, n = (s - 1); n > -1; --n)
	{
		b = (i >> (n * 4)) & 0xf;
		buf[z] = itoh_map[b];
		++z;
	}
	buf[z] = 0;
	return buf;
}

static void __ksprintf(char *buf, const char *fmt, __builtin_va_list argp)
{
	const char 				*p;
	int 					i;
	char 					*s;
	char 					fmtbuf[256];
	int						x, y;

	//__builtin_va_start(argp, fmt);
	
	x = 0;
	for(p = fmt; *p != '\0'; p++)
	{
		if (*p == '\\') {
			switch (*++p) {
				case 'n':
					buf[x++] = '\n';
					break;
				default:
					break;
			}
			continue;
		}
	
		if(*p != '%')
		{
			buf[x++] = *p;
			continue;
		}

		switch(*++p)
			{
			case 'c':
				i = __builtin_va_arg(argp, int);
				buf[x++] = i;
				break;
			case 's':
				s = __builtin_va_arg(argp, char*);
				for (y = 0; s[y]; ++y) {
					buf[x++] = s[y];
				}
				break;
			case 'x':
				i = __builtin_va_arg(argp, int);
				s = itoh(i, fmtbuf);
				for (y = 0; s[y]; ++y) {
					buf[x++] = s[y];
				}
				break;
			case '%':
				buf[x++] = '%';
				break;
		}
	}
	
	//__builtin_va_end(argp);
	buf[x] = 0;
}

void sprintf(char *buf, const char *fmt, ...) {
	__builtin_va_list		argp;
	
	__builtin_va_start(argp, fmt);
	__ksprintf(buf, fmt, argp);
	__builtin_va_end(argp);
}

void printf(const char *fmt, ...) {
	char					buf[128];
	__builtin_va_list		argp;
	
	__builtin_va_start(argp, fmt);
	__ksprintf(buf, fmt, argp);
	kserdbg_puts(buf);
	__builtin_va_end(argp);
}

uint32 getTicksPerSecond() {
	uint32			out;
	asm("	swi #103 \n\
			mov %[out], r0 \n\
		" : [out]"=r" (out));
	return out;
}

/*
	TODO: Maybe find a more efficent way than doing two system calls back to back. Maybe have a special system
	      call that does both and takes second as an argument. At the moment I am just wanting to keep the kernel
		  minimal and leave optimization for later on down the road if needed.
*/
int sleep(uint32 timeout) {
	int			result;
	uint32		tps;
	
	/* convert to ticks */
	tps = getTicksPerSecond();
	timeout = timeout * tps;
	
	asm("	mov r0, %[in] \n\
			swi #101 \n\
			mov %[result], r0 \n\
		" : [result]"=r" (result) : [in]"r" (timeout));
	/* convert from ticks */
	//return result / tps;
	return result;
}

void yield() {
	asm("swi #102");
}

/*
	This will copy a message into a buffer and WILL block until a message has been read, or until
	the timeout expires.
*/
int rb_read_bio(RBM volatile *rbm, void *p, uint32 *sz, uint32 *advance, uint32 timeout) {
	while (!rb_read_nbio(rbm, p, sz, advance)) {
		/* if timeout expired then exit */
		if (!sleep(timeout)) {
			break;
		}
	}
}

RBM				__corelib_rx;
RBM				__corelib_tx;

/*
void __attribute__((naked)) _start() {
	asm("mov r3, #0");
	for (;;);
	asm("mov r1, #0x16000000");
	asm("mov r2, sp");
	asm("str r2, [r1]");
	for(;;);
}*/

//int rb_read_bio(RBM volatile *rbm, void *p, uint32 *sz, uint32 *advance, uint32 timeout) {
void _start(uint32 rxaddr, uint32 txaddr, uint32 txrxsz) {
	/* setup meta data (outside shared memory / ring buffer) for protected fields */
	printf("rxaddr:%x txaddr:%x txrxsz:%x\n", rxaddr, txaddr, txrxsz);
	
	__corelib_rx.sz = txrxsz;
	__corelib_rx.rb = (RB*)rxaddr;
	__corelib_tx.sz = txrxsz;
	__corelib_tx.rb = (RB*)txaddr;
	
	/* wait to read arguments from ring buffer */
	//rb_read_bio(&__corelib_rx, 
	
	main();
}