#include "core.h"
#include "rb.h"
#include "main.h"

#define SERIAL_BASE 0xa0000000
#define SERIAL_FLAG_REGISTER 0x18
#define SERIAL_BUFFER_FULL (1 << 5)

static unsigned long int next = 1;
 
void memset(void *p, uint8 v, uintptr sz) {
	uint8 volatile		*_p;
	uintptr				x;
	
	_p = (uint8*)p;
	
	for (x = 0; x < sz; ++x) {
		_p[x] = v;
	}
	
	return;
}

void memcpy(void *_a, void *_b, uintptr sz) {
	uint8		*a;
	uint8		*b;
	uintptr		x;
	
	a = (uint8*)_a;
	b = (uint8*)_b;
	
	for (x = 0; x < sz; ++x) {
		a[x] = b[x];
	}
}
 
uint32 __rand(uint32 next) {
    next = next * 1103515245 + 12345;
    return (uint32)(next / 65536) % 32768;
}
 
int rand() {
	return (int)__rand(next);
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

int __attribute__((naked)) signal(uintptr proc, uintptr thread, uintptr signal) {
	asm volatile (
		"swi %[code]\n"
		"bx lr\n"
		: : [code]"i" (KSWI_SIGNAL)
	);
}

//r0, r1, r2
int __attribute__((naked)) getsignal(uintptr *process, uintptr *signal) {
	asm volatile (
		"push {r3, r4}\n"
		"mov r3, r0\n"
		"mov r4, r1\n"
		"swi %[code]\n"
		"str r1, [r3]\n"
		"str r2, [r4]\n"
		"pop {r3, r4}\n"
		"bx lr\n"
		: : [code]"i" (KSWI_GETSIGNAL)
	);
}

void printf(const char *fmt, ...) {
	char					buf[128];
	__builtin_va_list		argp;
	
	__builtin_va_start(argp, fmt);
	__ksprintf(buf, fmt, argp);
	kserdbg_puts(buf);
	__builtin_va_end(argp);
}

uint32 __attribute((naked)) getTicksPerSecond() {
	asm volatile (
		"swi #103\n"
		"bx lr\n"
	);
}

/*
	TODO: Maybe find a more efficent way than doing two system calls back to back. Maybe have a special system
	      call that does both and takes second as an argument. At the moment I am just wanting to keep the kernel
		  minimal and leave optimization for later on down the road if needed.
*/

uint32 __attribute__((noinline)) sleepticks(uint32 timeout) {
	uint32			result;
	asm volatile (
			"mov r0, %[in] \n"
			"swi #101 \n"
			"mov %[result], r0 \n"
			: [result]"=r" (result) : [in]"r" (timeout));
	return result;
}

int sleep(uint32 timeout) {
	int			result;
	uint32		tps;
	
	/* convert to ticks */
	tps = getTicksPerSecond();
	
	timeout = timeout * tps;
	
	result = sleepticks(timeout);
	/* convert from ticks */
	return result / tps;
}

int strcmp(char *a, char *b) {
	uint32			x;

	for (x = 0; (a[x] != 0) && (b[x] != 0); ++x) {
		if (a[x] > b[x]) {
			return 1;
		}
		if (b[x] > a[x]) {
			return -1;
		}
	}
	
	if (a[x] == b[x]) {
		return 0;
	}
	
	if (a[x] == 0) {
		return -1;
	}
	
	return 1;
}

int strlen(char *str) {
	int			x;
	
	for (x = 0; str[x] != 0; ++x);
	
	return x;
}

uint32 getosticks() {
	asm("swi %[code]" : : [code]"i" (KSWI_GETOSTICKS));
}

void switchto(uintptr	proc, uintptr thread) {
	asm volatile (
			"push {r0, r1}\n"
			"swi %[code]\n"
			"pop {r0, r1}\n"
			: : [code]"i" (KSWI_SWITCHTO)
	);
}

void wakeup(uintptr	proc, uintptr thread) {
	asm volatile (
			"push {r0, r1}\n"
			"swi %[code]\n"
			"pop {r0, r1}\n"
			: : [code]"i" (KSWI_WAKEUP)
	);
}

void notifykserver() {
	asm("swi %[code]" : : [code]"i" (KSWI_KERNELMSG));
}

void yield() {
	asm volatile (
		"swi %[code]"
		: : [code]"i" (KSWI_YIELD)
	);
}

uintptr __attribute__((naked)) valloc(uintptr cnt) {
	asm volatile (
			"swi %[code]\n"
			"bx lr\n"
			: : [code]"i" (KSWI_VALLOC)
		);
}

void __attribute__((naked)) vfree(uintptr addr, uintptr cnt) {
	asm volatile (
		"swi %[code]\n"
		"bx lr\n"
		: : [code]"i" (KSWI_VFREE)
	);
}

int __attribute__((naked)) getvirtref(uintptr v) {
	asm volatile (
		"swi %[code]\n"
		"bx lr\n"
		: : [code]"i" (KSWI_GETVIRTREF)
	);
}

uintptr __attribute__((naked)) getpagesize() {
	asm volatile ("swi %[code]\nbx lr\n" : : [code]"i" (KSWI_GETPAGESIZE));
}

ERH				__corelib_rx;
ERH				__corelib_tx;
KHEAPBM			__corelib_heap;

void *malloc(uint32 size) {
	void			*ptr;
	uint32			_size;	
	uintptr			pagesize;
	uintptr			minheapblksz;
	
	/* attempt first allocation try (will fail on very first) */
	ptr = k_heapBMAlloc(&__corelib_heap, size);
	
	/* try adding more memory if failed */
	if (!ptr) {
		pagesize = getpagesize();
		minheapblksz = 4;
		
		if (size < (minheapblksz * pagesize) / 2) {
			/* we need to allocate blocks at least this size */
			_size = minheapblksz;
		} else {
			/* this is bigger than KCHKMINBLOCKSZ, so lets double it to be safe */
			/* round up allocation to use all the blocks taken */
			_size = size * 2;
			_size = (_size / pagesize) * pagesize < _size ? _size / pagesize + 1 : _size / pagesize;
		}
		
		/* allocate more memory and add block */
		ptr = (void*)valloc(_size);
		
		if (!ptr) {
			return 0;
		}
		
		k_heapBMAddBlock(&__corelib_heap, (uintptr)ptr, _size * KPHYPAGESIZE, KCHKHEAPBSIZE);
		
		/* try allocation once more, should succeed */
		ptr = k_heapBMAlloc(&__corelib_heap, size);
		if (!ptr) {
			return 0;
		}
	}
	
	return ptr;
}

void free(void *ptr) {
	k_heapBMFree(&__corelib_heap, ptr);
}

/*
	@sdescription:		The entry point for the application.
	@ldescription:		This sets up the kernel IPC link, and heap.
*/
void _start(uint32 rxaddr, uint32 txaddr, uint32 txrxsz) {
	uintptr			heapoff;
	uintptr			sp;

	printf("rxaddr:%x txaddr:%x txrxsz:%x\n", rxaddr, txaddr, txrxsz);
	
	memset(&__corelib_rx, 0, sizeof(ERH));
	memset(&__corelib_tx, 0, sizeof(ERH));
	
	//memset(&__tmp[0], 0, sizeof(__tmp));
	
	/* allocate initial heap at about 4 virtual pages with 16 byte blocks */
	k_heapBMInit(&__corelib_heap);
	heapoff = valloc(4);
	printf("#heapoff:%x\n", heapoff);
	((uint32*)heapoff)[0] = 0;
	printf("fptr:%x\n", &k_heapBMAddBlock);
	//printf("tmp:%x\n", &__tmp[0]);
	printf("__corelib_heap:%x\n", &__corelib_heap);
	//((uint32*)0xa0000000)[0] = '%';
	asm volatile ("mov %[out], sp" : [out]"=r" (sp));
	printf("sp:%x\n", sp);
	k_heapBMAddBlock(&__corelib_heap, heapoff, 4 * getpagesize(), 16);
	printf("done adding heap block\n");
		
	/* ready our encapsulating structures for the kernel link */
	er_ready(&__corelib_rx, (void*)rxaddr, txrxsz, 16 * 4, 0);
	er_ready(&__corelib_tx, (void*)txaddr, txrxsz, 16 * 4, &katomic_lockspin_yield8nr);
	
	/* launch the next layer of application because we are done here */
	main();
	
	/* TODO: add termination */
	for (;;);
}