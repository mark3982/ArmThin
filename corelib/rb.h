#ifndef CORELIB_RB_H
#define CORELIB_RB_H
#include "stdtypes.h"

struct _RBE {
	int32			w;
	int32			r;
	uint8			d[];
};

typedef struct _RBE RB;

struct _RBME {
	RB				*rb;
	int32			sz;
};

typedef struct _RBME RBM;

typedef int (*KATOMIC_LOCKSPIN8NR)(volatile uint8 *ptr, uint8 id);

typedef struct _ERH {
	uintptr				rproc;			/* remote proc */
	uintptr				rthread;		/* remote thread */
	void				*er;			/* map and data area */
	uint32				tsz;			/* total size */
	uint32				esz;			/* entry size */
	uint32				ecnt;			/* total entries */
	uint32				mcnt;			/* entries used by map */
	uint32				lpos;			/* last position */
	KATOMIC_LOCKSPIN8NR	lockfp;			/* locking function pointer */
	uintptr				signal;			/* signal remote thread wants to get */
} ERH;

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

#define IPC_PROTO_RB		0x100
#define IPC_PROTO_ER		0x200

int rb_write_nbio(RBM *rbm, void *p, uint32 sz);
int rb_read_nbio(RBM *rbm, void *p, uint32 *sz, uint32 *advance);
int rb_read_bio(RBM *rbm, void *p, uint32 *sz, uint32 *advance, uint32 timeout);

int er_init(ERH * erh, void *data, uint32 tsz, uint32 esz, KATOMIC_LOCKSPIN8NR lockfp);
int er_ready(ERH *erh, void *data, uint32 tsz, uint32 esz, KATOMIC_LOCKSPIN8NR lockfp);
int er_write_nbio(ERH *erh, void *p, uint32 sz);
int er_read_nbio(ERH *erh, void *p, uint32 *sz);
int er_peek_nbio(ERH *erh, void *p, uint32 *sz, uint8 **mndx);
#endif