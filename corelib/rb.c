#include "core.h"
#include "rb.h"
#include <atomic.h>

#ifdef KERNEL
#define printf kprintf
#endif

#define ROUNDUPINTDIV(a, b) ((a / b) * b < a ? (a / b) + 1 : a / b)

int er_ready(ERH *erh, void *data, uint32 tsz, uint32 esz, KATOMIC_LOCKSPIN8NR lockfp) {		
	uint32			ecnt;
	uint32			mcnt;
	
	ecnt = tsz / esz;
	mcnt = ROUNDUPINTDIV(ecnt, esz);
	
	erh->lockfp = lockfp;
	erh->er = data;
	erh->tsz = tsz;
	erh->esz = esz;
	erh->ecnt = ecnt;
	erh->mcnt = mcnt;
	erh->lpos = 0;
	return 1;
}

int er_init(ERH * erh, void *data, uint32 tsz, uint32 esz, KATOMIC_LOCKSPIN8NR lockfp) {
	uint8 volatile	*map;
	uint32			x;

	if (!er_ready(erh, data, tsz, esz, lockfp)) {
		return 0;
	}
	
	map = (volatile uint8*)erh->er;
	
	/* permanently locked */
	for (x = 0; x < erh->mcnt; ++x) {
		map[x] = 0x7f;
	}
	
	/* open for usage */
	for (; x < erh->ecnt; ++x) {
		map[x] = 0x00;
	}
	return 1;
}


int er_write_nbio(ERH *erh, void *p, uint32 sz) {
	uint8				*map;
	uint32				x, y;
	uint8				*data;
	uint32				ecnt;
	KATOMIC_LOCKSPIN8NR	lockfp;
	void				*er;
	uint32				esz;
	
	er = erh->er;
	ecnt = erh->ecnt;
	lockfp = erh->lockfp;
	esz = erh->esz;
	
	map = (uint8*)erh->er;
	
	//if (!erh->lockfp) {
	//	printf("[er] no lockfp!\n");
	//	return -2;
	//}
	
	if (sz > esz) {
		printf("[er] sz:%x > erh->esz:%x\n", sz, erh->esz);
		return -1;
	}
	
	/* find entry and lock it */
	for (x = 0; x < ecnt; ++x) {
		/* try to lock it */
		//printf("[er] <write> checking map[%x]:%x\n", x, map[x]);
		if (map[x] == 0) {
			if (lockfp && !lockfp(&map[x], 1)) {
				continue;
			}
			//printf("[er] wrote at %x\n", x);
			/* got lock, now write data */
			data = (uint8*)((uintptr)er + (x * esz));
			for (y = 0; y < sz; ++y) {
				data[y] = ((uint8*)p)[y];
			}
			/* set valid flag */
			map[x] = map[x] | 0x80;
			return 1;
		}
	}
	
	return 0;
}

int er_peek_nbio(ERH *erh, void *p, uint32 *sz, uint8 **mndx) {
	uint32			x, y;
	uint8			*map;
	uint8			*data;
	uint32			max;
	uint32			esz;
	uint32			lsz;
	void			*er;
	
	map = (uint8*)erh->er;
	
	max = erh->ecnt;
	esz = erh->esz;
	lsz = *sz;
	er = erh->er;
	
	for (x = 0; x < max; ++x) {
		//printf("[er] <read> checking map[%x]:%x\n", x, map[x]);
		if (map[x] & 0x80) {
			/* found entry, now copy out data */
			data = (uint8*)((uintptr)er + (x * esz));
			for (y = 0; (y < lsz) && (y < esz); ++y) {
				((uint8*)p)[y] = data[y];
			}
			/* set pointer to lockmap byte */
			*sz = y;
			*mndx = &map[x];
			return 1;
		}
	}
	/* keep erh->lpos the same */
	return 0;
}

int er_read_nbio(ERH *erh, void *p, uint32 *sz) {
	uint8		*me;
	
	if (er_peek_nbio(erh, p, sz, &me)) {
		/* clear entry so it can be used again */
		me[0] = 0;
		return 1;
	}
	/* nothing has been read */
	return 0;
}

int rb_ready(RBM *rbm, void *p, uint32 sz) {
	rbm->rb = (RB*)p;
	rbm->sz = sz;
}

int rb_write_nbio(RBM *rbm, void *p, uint32 sz) {
	RB volatile		*rb;
	int32			r;
	int32			w;
	uint32			*h;
	int32			x, y;
	int32			asz;
	int32			max;
	
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

	/* calculate total size including 16-bit length header */
	asz = sz + 2 + 2;
	
	/* not enough space */
	if ((w < r) && (w + asz) >= r) {
		return 0;
	}
	
	/* not enough space */
	if ((w >= r) && ((rbm->sz - w) + r) < asz) {
		return 0;
	}
	
	/* write length */
	rb->d[w++] = sz >> 8;
	if (w >= rbm->sz) {
		w = 0;
	}
	rb->d[w++] = sz & 0xff;
	if (w >= rbm->sz){
		w = 0;
	}
	
	/* split write */
	max = rbm->sz - w;
	if (w >= r && max < sz) {
		/* copy first part */
		for (x = 0; x < max; ++x) {
			rb->d[w + x] = ((uint8*)p)[x];
		}
		/* copy last part */
		for (y = 0; x < sz; ++x, ++y) {
			rb->d[y] = ((uint8*)p)[x];
		}
		
		rb->w = (w + sz) - rbm->sz;
		return 1;
	}
	
	/* straight write */
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

int rb_read_nbio(RBM *rbm, void *p, uint32 *sz, uint32 *advance) {
	RB volatile		*rb;
	int32			r;
	int32			w;
	int32			h;
	int32  			x, y;
	uint8			*_p;
	int32			max;
	
	_p = (uint8*)p;
		
	rb = (RB volatile*)rbm->rb;
	
	r = rb->r;
	w = rb->w;
	
	if (advance) {
		r = *advance;
	} else {
		if (r > rbm->sz) {
			/* bad header */
			return 0;
		}
	}
	
	if (w > rbm->sz) {
		/* bad header */
		return 0;
	}
	
	if (w == r) {
		return 0;
	}
	
	/* read size (tricky) */
	h = rb->d[r++] << 8;
	if (r == w) {
		return 0;
	}
	if (r >= rbm->sz) {
		r = 0;
	}
	h |= rb->d[r++];
	if (r == w) {
		return 0;
	}
	if (r >= rbm->sz) {
		r = 0;
	}
	
	if (h > (rbm->sz - r) + r) {
		return -1;
	}
	
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
	
	if (advance) {
		*advance = r + h;
	} else {
		rb->r = r + h;
	}
	
	return 1;
}