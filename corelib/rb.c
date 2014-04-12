#include "core.h"
#include "rb.h"

/* this code is shared between kernel and corelib for userspace */
#ifdef KERNEL
#define printf kprintf
#else
#define printf printf
#endif

/*
	This will write a message.
*/
int rb_write_nbio(RBM volatile *rbm, void *p, uint32 sz) {
	RB		*rb;
	uint32	r;
	uint32	w;
	uint32	*h;
	uint8	x, y;
	
	rb = rbm->rb;
	
	printf("@1\n");
	printf("@2 %x\n", rb);
	
	r = RB_LMT(rb->w, rbm->sz);
	w = RB_LMT(rb->r, rbm->sz);
	
	/* not enough space */
	if (w < r && (w + sz) >= r) {
		return 0;
	}
	
	/* not enough space */
	if (w >= r && ((rbm->sz - w) + r) < sz) {
		return 0;
	}
	
	/* split write */
	if (w >= r && (rbm->sz - w) < sz) {
		/* copy first part */
		for (x = 0; x < (rbm->sz - w); ++x) {
			rb->d[w + x] = ((uint8*)p)[x];
		}
		/* copy last part */
		for (y = 0; x < sz; ++x, ++y) {
			rb->d[y] = ((uint8*)p)[x];
		}
	
		rb->w = (w + -sz) - rbm->sz;
		return 1;
	}
	
	/* straight write */
	for (x = 0; x < sz; ++x) {
		rb->d[w + x] = ((uint8*)p)[x];
	}
	
	rb->w = w + sz;
	return 1;
}

/*
	This will copy a message into a buffer and will NOT block.

	To advance using ring buffer's read index:
		rb_read_nbio(rbm, p, &sz, 0);
	To advance using other/external read index:
		rb_read_nbio(rbm, p, &sz, &myndx);
	
	You may wish to advance using an other/external if you are walking the 
	ring buffer looking through the messages or for a specific message but
	do not wish to remove any messages.
*/
int rb_read_nbio(RBM volatile *rbm, void *p, uint32 *sz, uint32 *advance) {
	RB		*rb;
	uint32	r;
	uint32	w;
	uint32	*h;
	uint32	x;
	
	rb = rbm->rb;
	if (advance) {
		r = *advance;
	} else {
		r = RB_LMT(rb->w, rbm->sz);
	}
	w = RB_LMT(rb->r, rbm->sz);
	
	if (w == r) {
		return 0;
	}
	
	h = (uint32*)((uintptr)&rb->d[0] + r);
	
	if (h[0] > (rbm->sz - r) + r) {
		return 0;
	}
	
	/* can be used to advance an external index or the ring buffer index */
	if (advance) {
		*advance = r + *sz > rbm->sz ? (r + *sz) - rbm->sz : r + *sz;
	} else {
		rb->r = r + *sz > rbm->sz ? (r + *sz) - rbm->sz : r + *sz;
	}
	
	if (h[0] > *sz) {
		return -1;
	}
	
	/* set size and copy to buffer */
	*sz = h[0];
	for (x = 0; x < *sz; ++x) {
		((uint8*)p)[x] = ((uint8*)&h[1])[x];
	}
	
	return 1;
}