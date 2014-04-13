#include "core.h"
#include "rb.h"

/* this code is shared between kernel and corelib for userspace */
#ifdef KERNEL
#define printf kprintf
#else
#define printf printf
#endif

#define RB_LMT(x, sz) (x > sz ? (x - sz > sz ? 0 : x - sz) : x)

/*
	This will write a message.
*/
int rb_write_nbio(RBM *rbm, void *p, uint32 sz) {
	RB volatile		*rb;
	uint32			r;
	uint32			w;
	uint32			*h;
	uint8			x, y;
	uint32			asz;
	
	rb = (RB volatile*)rbm->rb;
	
	sz = sz & 0xffff;
	
	r = rb->r;
	w = rb->w;
	
	if (r > rbm->sz) {
		return 0;
	}
	
	if (w > rbm->sz) {
		return 0;
	}
	
	//printf("[rb] r:%x w:%x\n", rb->r, rb->w);

	/* calculate total size including 16-bit length header */
	asz = sz + 2 + 2;
	
	/* not enough space */
	if ((w < r) && (w + asz) >= r) {
		//printf("##$#\n");
		return 0;
	}
	
	/* not enough space */
	if ((w >= r) && ((rbm->sz - w) + r) < asz) {
		//printf("LESS THAN\n");
		return 0;
	}
	
	//printf("write size:%x w:%x max:%x\n", sz, w, rbm->sz);
	/* write length */
	if (w == rbm->sz) {
		w = 0;
	}
	rb->d[w++] = sz >> 8;
	if (w >= rbm->sz) {
		w = 0;
	}
	rb->d[w++] = sz & 0xff;
	if (w >= rbm->sz){
		w = 0;
	}
	
	//printf("HERE!!!\n");
	
	/* split write */
	if (w >= r && (rbm->sz - w) < sz) {
		//printf("[rb] split write\n");
		/* copy first part */
		for (x = 0; x < (rbm->sz - w); ++x) {
			rb->d[w + x] = ((uint8*)p)[x];
		}
		/* copy last part */
		for (y = 0; x < sz; ++x, ++y) {
			rb->d[y] = ((uint8*)p)[x];
		}
		
		//printf("split-write final; cur-w:%x new-w:%x\n", rb->w, (w + sz) - rbm->sz);
		rb->w = (w + sz) - rbm->sz;
		return 1;
	}
	
	/* straight write */
	//printf("[rb] straight write w:%x sz:%x\n", w, sz);
	for (x = 0; x < sz; ++x) {
		rb->d[w + x] = ((uint8*)p)[x];
	}
	
	/* 
		split read wont leave 'w' == rbm->sz but this will so we have
		to check for it and correct it else it messed up the reader 
		getting them off-track and essentially making communications
		hard to reliably recover if not impossible
	*/
	if (w + sz == rbm->sz) {
		rb->w = 0;
	} else {
		rb->w = w + sz;
	}
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
int rb_read_nbio(RBM *rbm, void *p, uint32 *sz, uint32 *advance) {
	RB volatile		*rb;
	uint32			r;
	uint32 volatile	w;
	uint32			h;
	int  			x, y;
	uint8			*_p;
	uint32			max;
	
	_p = (uint8*)p;
	
	rb = (RB volatile*)rbm->rb;
	
	r = rb->r;
	w = rb->w;
	
	if (advance) {
		r = *advance;
	} else {
		if (r > rbm->sz) {
			/* bad header */
			printf("  bad hdr\n");
			return 0;
		}
	}
	
	if (w > rbm->sz) {
		/* bad header */
		printf("  bad hdr\n");
		return 0;
	}
	
	if (w == r) {
		//printf("   w == r\n");
		return 0;
	}
	
	//printf("OKOKOK r:%x w:%x rbm->sz:%x\n", r, w, rbm->sz);
	/* read size (tricky) */
	//printf("r:%x rbm->sz:%x\n", r, rbm->sz);
	h = rb->d[r++] << 8;
	if (r == w) {
		printf(" on h read high r==w\n");
		return 0;
	}
	if (r >= rbm->sz) {
		r = 0;
	}
	h |= rb->d[r++];
	if (r == w) {
		printf(" on h read low r==w\n");
		return 0;
	}
	if (r >= rbm->sz) {
		r = 0;
	}
	
	if (h > (rbm->sz - r) + r) {
		for (x = -20; x < 20; ++x) {
			if ((x + (int)rb->r) < 0) {
				y = rbm->sz + (x + (int)rb->r);
			} else {
				y = x + rb->r;
			}
			printf("   [%x]:%x\n", y, rb->d[y]);
		}
	
		printf(" h too big as h:%x rb->r:%x rb->w:%x\n", h, rb->r, rb->w);
		for (;;);
		return -1;
	}
	
	//printf("  advancing h:%x\n", h);
	
	if (h > *sz) {
		return -1;
	}
	
	*sz = h;

	/* split read */
	if (r + h >= rbm->sz) {
		max = rbm->sz - r;
		for (x = 0; x < max; ++x) {
			*(_p++) = rb->d[r + x];
		}
		
		max = h - (rbm->sz - r);
		for (x = 0; x < max; ++x) {
			*(_p++) = rb->d[x];
		}
		//printf("[split-read] sz:%x max:%x old-r:%x new-r:%x\n", h, rbm->sz, rb->r, (r + h) - rbm->sz);
		if (advance) {
			*advance = (r + h) - rbm->sz;
		} else {
			rb->r = (r + h) - rbm->sz;
		}
		return 1;
	}
	
	/* straight read */
	for (x = 0; x < h; ++x) {
		*(_p++) = rb->d[r + x];
	}
	
	//printf("[straight-read] h:%x old-r:%x new-r:%x cur-w:%x\n", h, rb->r, r + h, rb->w);
	if (advance) {
		*advance = r + h;
	} else {
		rb->r = r + h;
	}
	
	return 1;
}