#ifndef CORELIB_CORE_H
#define CORELIB_CORE_H
#include "../stdtypes.h"

typedef struct _RB {
	uint32			w;
	uint32			r;
	uint8			d[];
} RB;

typedef struct _RBM {
	RB				*rb;
	uint32			sz;
} RBM;

/*
	the writer can write if r==w
	the writer has to wait if w < r and sz > (r - w)
    the writer has to wait if w >= r and ((mask - w) + r) < sz
	
	the reader has to wait if r == w
	the reader can read during any other condition
	
	POINTS
			- research memory barrier to prevent modification of index
			  before data is written into the buffer
			- consider two methods for sleeping.. one using a flag bit
			  for the thread/process and the other using a lock
			  
	
	[signal objects]
	- kernel objects can be created by a process that are signals
	- other processes can wait on these objects (but only the creating process can signal them)
	- object is specified with processID:signalID
*/

#define RB_LMT(x, sz) (x > sz ? (x - sz > sz ? 0 : x - sz) : x)

int rb_read_nbio(RBM volatile *rbm, void *p, uint32 *sz, uint32 *advance);
int rb_read_bio(RBM volatile *rbm, void *p, uint32 *sz, uint32 *advance, uint32 timeout);
int sleep(uint32 timeout);
void yield();
uint32 getTicksPerSecond();

void printf(const char *fmt, ...);
void sprintf(char *buf, const char *fmt, ...);
char* itoh(int i, char *buf);
#endif