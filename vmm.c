#include "main.h"
#include "vmm.h"

/*
	This place code at entry and exit paths for external functions that are callable. 
	
*/
#ifdef KVMMLOCK
#define VMMKCCENTER KCCENTER
#define VMMKCCEXIT KCCEXIT
#else
#define VMMKCCENTER
#define VMMKCCEXIT
#endif

int kstack_initblock(KSTACKBLOCK *b) {
	uint32			x;
	/* set max and top and prev */
	b->max = (K1KPAGESTACKBSZ - sizeof(KSTACKBLOCK)) / 4;
	b->top = 0;
	
	/* clear stack */
	for (x = 0; x < b->max; ++x) {
		((uint32*)&b[1])[x] = 0;
	}
	
	return 1;
}
int kstack_empty(KSTACK *stack) {
	if (!stack->cur->top && !stack->cur->prev) {
		return 1;
	}

	return 0;
}

int kstack_pop(KSTACK *stack, uint32 *v) {
	if (!stack->cur->top && !stack->cur->prev) {
		return 0;
	}
	
	if (stack->cur->top == 0) {
		stack->cur = stack->cur->prev;
	}

	*v = ((uint32*)&stack->cur[1])[--stack->cur->top];
}

int kstack_push(KSTACK *stack, uint32 v) {
	KSTACKBLOCK				*b, *_b;
	KSTATE					*ks;
	
	ks = GETKS();
	
	b = stack->cur;
	
	if (b->top == b->max) {
		/* alloc another block */
		if (!b->next) {
			_b = (KSTACKBLOCK*)k_heapBMAllocBound(&ks->hchk, K1KPAGESTACKBSZ, 0);
			if (!_b) {
				PANIC("unable-to-create-1k-stack");
			}
		}
		/* link */
		_b->prev = b;
		_b->next = 0;
		b->next = _b;
		stack->cur = _b;
		
		ASSERTPANIC(_b, "KSTACK UNABLE TO CREATE NEW BLOCK");
		
		/* swap and init */
		kstack_initblock(_b);
		b = _b;
	}
	
	((uint32*)&b[1])[b->top++] = v;
}

/*
	This function does not support being called when paging is enabled. It however will
	gracefully fail if unable to initialize. It may initialize with paging on but it is
	not reliable.
*/
int kstack_init(KSTACK *stack) {
	KSTACKBLOCK				*b;
	KSTATE					*ks;
	uint32					x;
	
	ks = GETKS();
	/* paging is not likely up so we have to identity map */
	b = (KSTACKBLOCK*)k_heapBMAllocBound(&ks->hchk, K1KPAGESTACKBSZ, 0);
	if (!b) {
		PANIC("heap-alloc-failed");
		return 0;
	}
	
	/* link stack */
	stack->cur = b;
	b->prev = 0;
	b->next = 0;
	
	kstack_initblock(b);
	return 1;
}

uintptr kvmm2_rndup(uintptr sz) {
	return (sz / 4096) * 4096 != sz ? (sz / 4096) + 1 : sz / 4096;
}

uint32 kvmm2_revinc(uintptr v) {
	uint32		val;
	val = kvmm2_revget(v, 1) + 1;
	kvmm2_revset(v, val, 1);
	return val;
}

uint32 kvmm2_revdec(uintptr v) {
	uint32		val;
	
	val = kvmm2_revget(v, 1);
	if (val == 0) {
		return val;
	}
	--val;
	kvmm2_revset(v, val, 1);
	return val;
}


int kvmm2_revset(uintptr p, uint32 v, uint8 opt) {
	KSTATE			*ks;
	uint32			*t;
	
	ks = GETKS();
	
	VMMKCCENTER(&ks->revlock);

	/* add table to reverse map */
	if (!ks->vmm_rev[p >> 20]) {
		/* 1K granularity */
		ks->vmm_rev[p >> 20] = (uint32)kmalloc(4096);
	}
	
	/* get sub-table */
	t = (uint32*)ks->vmm_rev[p >> 20];
	
	if (!opt) {
		/* keep lower and replace upper */
		t[(p >> 10) & 0x3ff] = (t[(p >> 10) & 0x3ff] & 0x3ff) | v;
		VMMKCCEXIT(&ks->revlock);
		return 1;
	}
	/* keep upper and replace lower */
	t[(p >> 10) & 0x3ff] = (t[(p >> 10) & 0x3ff] & ~0x3ff) | v;
	VMMKCCEXIT(&ks->revlock);
	return 1;
}

uintptr kvmm2_revget(uintptr p, uint8 opt) {
	uint32			*t;
	KSTATE			*ks;
	uintptr			res;
	
	ks = GETKS();
	
	VMMKCCENTER(&ks->revlock);
	
	if (!ks->vmm_rev[p >> 20]) {
		kprintf("error:revget; p:%x\n", p);
		PANIC("revget-no-sub-table");
		VMMKCCEXIT(&ks->revlock);
		return 0;
	}
	
	t = (uint32*)ks->vmm_rev[p >> 20];
	
	if (!opt) {
		res = t[(p >> 10) & 0x3ff] & ~0x3ff;
		VMMKCCEXIT(&ks->revlock);
		return res;
	}

	res = t[(p >> 10) & 0x3ff] & 0x3ff;
	VMMKCCEXIT(&ks->revlock);
	return res;
}

int kvmm2_getu4k(KVMMTABLE *vmm, uintptr *o, uint32 flags) {
	int32			x, y;
	uint32			*t, *st;
	uint32			sp;
	
	VMMKCCENTER(&vmm->lock);
	
	t = (uint32*)vmm->table;
		
	/* check 1MB coarse table entries */
	for (x = 4095; x > -1; --x) {
		if ((t[x] & 3) != 0) {
			/* get physical address of level 2 table */
			st = (uint32*)(t[x] & ~0x3ff);
			if (!(flags & KVMM_DIRECT)) {
				/* translate into virtual address */
				st = (uint32*)kvmm2_revget((uintptr)st, 0);
				if (!st) {
					PANIC("unusual-zero-addr-for-table");
				}
			}
			/* look through table for empty entry */
			for (y = 255; y > -1; --y) {
				if ((st[y] & 3) == 0) {
					*o = (x << 20) | (y << 12);
					kprintf("*o:%x\n", *o);
					VMMKCCEXIT(&vmm->lock);
					return 1;
				}
			}
		}
	}
	VMMKCCEXIT(&vmm->lock);
	return 0;
}

/* get unused coarse table slot */
uint32 kvmm2_getucts(KVMMTABLE *vmm, uint32 *slot) {
	uint32			x;
	uint32			*t;
	
	VMMKCCENTER(&vmm->lock);
	
	t = (uint32*)vmm->table;
	
	/* check 1MB coarse table entries */
	for (x = 0; x < 4096; ++x) {
		if ((t[x] & 3) == 0) {
			*slot = x;
			VMMKCCEXIT(&vmm->lock);
			return 1;
		}
	}
	
	VMMKCCEXIT(&vmm->lock);
	return 0;
}

/*
	NOT PROTECTED BY A PER-CPU LOCK
*/
static int kvmm2_get1Ktable(uintptr *o, uint32 flags) {
	KSTATE			*ks;
	uint32			*ptaddr, *vtaddr;
	uint32			x;
	uintptr			fvaddr;
	uint32			*t;
	uint32			slot;
	uint8			latemap, latemap2;
	uintptr			lm_ptaddr, lm_vtaddr, lm2_ptaddr, lm2_vtaddr;
	
	ks = GETKS();
	
	t = ks->vmm.table;
	latemap = 0;
	
	/* we only need to know to use direct or not */
	if (flags & KVMM_DIRECT) {
		flags = KVMM_DIRECT;
	} else {
		flags = 0;
	}
	
	// check table stack.. is empty?
	if (kstack_empty(&ks->tstack)) {
		// check ucte (if less than 2 left)
		if (ks->vmm_ucte < 2) {
			/* get free table slot */
			kvmm2_getucts(&ks->vmm, &slot);
			//kprintf("slot:%x\n", slot);
			/* are we at the end of our address space */
			if ((slot << 20) > KMEMSIZE) {
				/* going outside of kernel address space */
				//kprintf("slot:%x\n", slot);
				PANIC("(slot << 30) > KMEMSIZE");
				return 0;
			}
			ptaddr = (uint32*)k_heapBMAllocBound(&ks->hphy, 1024, 10);
			t[slot] = (uintptr)ptaddr | TLB_COARSE;
			if (!(flags & KVMM_DIRECT)) {
				/* get empty 1K entry address */
				if (!kvmm2_getu4k(&ks->vmm, (uintptr*)&vtaddr, flags)) {
					PANIC("getu4k-failed");
				}
				/* set reverse map */
				kvmm2_revset((uintptr)ptaddr, (uintptr)vtaddr, 0);
				/* map page into space */
				kvmm2_mapsingle(&ks->vmm, (uintptr)vtaddr, (uintptr)ptaddr, flags | TLB_C_AP_PRIVACCESS);
			} else {
				vtaddr = ptaddr;
				kvmm2_revset((uintptr)ptaddr, (uintptr)vtaddr, 0);
				latemap = 1;
				lm_ptaddr = (uintptr)ptaddr;
				lm_vtaddr = (uintptr)vtaddr;
			}
			
			/* clear the table */
			for (x = 0; x < 256; ++x) {
				vtaddr[x] = 0;
			}
			
			ks->vmm_ucte += 256;
		}
		
		/* alloc 4K but on a 4K boundary */
		ptaddr = (uint32*)k_heapBMAllocBound(&ks->hphy, 4096, 12);
		if (!(flags & KVMM_DIRECT)) {
			/* map into memory and get virtual address */
			/* get empty 4K entry address */
			kvmm2_getu4k(&ks->vmm, (uintptr*)&vtaddr, flags);
			/* map page into space */
			kvmm2_mapsingle(&ks->vmm, (uintptr)vtaddr, (uintptr)ptaddr, flags | TLB_C_AP_PRIVACCESS);
			latemap2 = 0;
		} else {
			latemap2 = 1;
			vtaddr = ptaddr;
			lm2_vtaddr = (uintptr)vtaddr;
			lm2_ptaddr = (uintptr)ptaddr;
		}
		// add 4-1K pages to stack
		for (x = 0; x < 4; ++x) {
			kvmm2_revset((uintptr)ptaddr + x * 0x400, (uintptr)vtaddr + x * 0x400, 0);
			kstack_push(&ks->tstack, (uintptr)vtaddr + 0x400 * x);
		}
		
		/* late map */
		if (latemap2) {
			kvmm2_mapsingle(&ks->vmm, lm2_vtaddr, lm2_ptaddr, flags | TLB_C_AP_PRIVACCESS);
		}
	}
	
	if (latemap) {
		if (!kvmm2_mapsingle(&ks->vmm, lm_vtaddr, lm_ptaddr, flags | TLB_C_AP_PRIVACCESS)) {
			PANIC("late-map-failed\n");
		}
	}
	// pop one (return value)
	kstack_pop(&ks->tstack, (uint32*)o);
	//kprintf("1KTable:%x\n", *o);
	return 1; 
}

int kvmm2_getphy(KVMMTABLE *vmm, uintptr v, uintptr *o) {
	uint32			*t;
	
	VMMKCCENTER(&vmm->lock);
	
	t = vmm->table;
	/* check not empty level one table entry */
	if ((t[v >> 20] & 3) == 0) {
		VMMKCCEXIT(&vmm->lock);
		return 0;
	}
	
	/* get sub table physical address */
	t = (uint32*)(t[v >> 20] & ~0x3ff);
	/* do reverse lookup to get virtual address */
	t = (uint32*)kvmm2_revget((uintptr)t, 0);
	
	/* make sure 4K page is actually mapped here... */
	if ((t[(v >> 12) & 0xff] & 0x3) != TLB_C_SMALLPAGE) {
		VMMKCCEXIT(&vmm->lock);
		return 0;
	}
	/* 
		also keep lower 12 bits of v address because we assume
		that they want the physical address not the actual physical
		page address
	*/
	*o = (t[(v >> 12) & 0xff] & ~0xfff) | (v & 0xfff); 
	VMMKCCEXIT(&vmm->lock);
	return 1;
}

int kvmm2_findregion(KVMMTABLE *vmm, uintptr tc, uintptr low, uintptr high, uint32 flags, uintptr *out) {
	uint32			*t, *st;
	KSTATE			*ks;
	uintptr			target;
	uintptr			c;
	uintptr			start;
	
	VMMKCCENTER(&vmm->lock);
	
	ks = GETKS();
	
	/* move low up if needed onto page boundary */
	low = low & 0xfff > 0 ? (low & ~0xfff) + 0x1000 : low & ~0xfff;
	/* drop high down if needed onto page boundary */
	if (!high) {
		high = ~0 & ~0xfff;
	} else {
		high = (high & ~0xfff) + 0x1000;
	}
	
	//kprintf("low:%x high:%x\n", low, high);
	
	t = vmm->table;
	c = 0;
	for (target = low, start = low; target < high; target += 0x1000) {
		if ((t[target >> 20] & 3) == 0) {
			c += 256;
			if (c >= tc) {
				*out = start;
				VMMKCCEXIT(&vmm->lock);
				return 1;
			}
			/* skip */
			target += 255 * 0x1000;
		} else {
			st = (uint32*)(t[target >> 20] & ~0x3ff);
			if (!(flags & KVMM_DIRECT)) {
				/* translate sub-table physical address into virtual */
				st = (uint32*)kvmm2_revget((uintptr)st, 0);
			}
			
			if ((st[(target >> 12) & 0xFF] & 3) == 0) {
				c += 1;
				if (c >= tc) {
					*out = start;
					VMMKCCEXIT(&vmm->lock);
					return 1;
				}
			} else {
				start = target + 0x1000;
				c = 0;
			}
		}
	}
	PANIC("region-not-found");
	VMMKCCEXIT(&vmm->lock);
	return 0;
}


int kvmm2_allocregion(KVMMTABLE *vmm, uintptr pcnt, uintptr low, uintptr high, uint32 flags, uintptr *out) {
	uint32			x;
	uintptr			p;
	uintptr			phy;
	KSTATE			*ks;
	
	VMMKCCENTER(&vmm->lock);
	
	ks = GETKS();
	
	if (!high) {
		high = low + pcnt * 0x1000;
	}
	
	low = low & ~0xFFF;
	
	if (flags & KVMM2_ALLOCREGION_NOFIND) {
		/* 
			this happens when we know were we want to map something, but
			it may already be mapped therefore the findregion would fail
			because it looks for a free contingious region of pages....
			
			so just pretend it found the range starting at low
		*/
		out[0] = low;
	} else {
		kprintf("find region low:%x high:%x\n", low, high);
		if (!kvmm2_findregion(vmm, pcnt, low, high, flags, out)) {
			VMMKCCEXIT(&vmm->lock);
			return 0;
		}
	}
	
	for (x = 0; x < pcnt; ++x) {
		if (flags & KVMM2_ALLOCREGION_NOFIND) {
			if (kvmm2_getphy(vmm, out[0] + x * 0x1000, &phy)) {
				/* okay its already been allocated and mapped */
				kprintf("   already mapped v:%x p:%x\n", out[0] + x * 0x1000, phy);
				continue;
			}
		}
		p = (uintptr)k_heapBMAllocBound(&ks->hphy, 0x1000, 12);
		if (!p) {
			PANIC("heap-alloc-failed");
			VMMKCCEXIT(&vmm->lock);
			return 0;
		}
		/* increment reference count */
		kvmm2_revinc(p);

		if (!kvmm2_mapsingle(vmm, out[0] + x * 0x1000, p, flags)) {
			PANIC("map-single-failed");
			VMMKCCEXIT(&vmm->lock);
			return 0;
		}
		kprintf("mapped %x -> %x\n", out[0] + x * 0x1000, p);
	}
	
	VMMKCCEXIT(&vmm->lock);
	return 1;
}

int kvmm2_allocregionat(KVMMTABLE *vmm, uintptr pcnt, uintptr start, uint32 flags) {
	uintptr				tmp;
	int					res;
	
	VMMKCCENTER(&vmm->lock);
	res = kvmm2_allocregion(vmm, pcnt, start, 0, flags, &tmp);
	VMMKCCEXIT(&vmm->lock);
	return res;
}

int kvmm2_mapsingle(KVMMTABLE *vmm, uintptr v, uintptr p, uint32 flags) {
	uint32			*t;
	uint32			*st;
	KSTATE			*ks;
	uintptr			phy;
	uint32			x;
	
	VMMKCCENTER(&vmm->lock);
	
	ks = GETKS();
	
	t = vmm->table;
	
	/* see if we need a page */
	if ((t[v >> 20] & 3) == 0) {
		/* maybe split this function into one to init/prep and one to actually get page */
		kvmm2_get1Ktable((uintptr*)&st, flags);
		/* TODO: put table back into stack if not used */
		kprintf("1K:%x\n", st);
	}
	
	if ((t[v >> 20] & 3) == 0) {
		//kprintf("using 1k table %x\n", st);
		/* update coarse table entries (ONLY for kernel space) */
		if (ks->vmm.table == t) {
			ks->vmm_ucte += 256;
		}
		if (!(flags & KVMM_DIRECT)) {
			/* reverse lookup table (use physical address) [keep st same] */
			if (!kvmm2_getphy(&ks->vmm, (uintptr)st, (uintptr*)&phy)) {
				PANIC("getphyfailed");
			}
			/* set table */
			t[v >> 20] = phy | TLB_COARSE;
			for (x = 0; x < 256; ++x) {
				st[x] = 0;
			}
			/* st is already virtual */
		} else {
			/* ks->vmm not ready so were in direct mode */
			t[v >> 20] = (uintptr)st | TLB_COARSE;
			for (x = 0; x < 256; ++x) {
				st[x] = 0;
			}
			/* st is already virtual/direct */
		}
	} else {
		st = (uint32*)(t[v >> 20] & ~0x3ff);
		if (!(flags & KVMM_DIRECT)) {
			/* get virtual from physical using reverse lookup */
			st = (uint32*)kvmm2_revget((uintptr)st, 0);
			if (!st) {
				PANIC("revgetfailed");
			}
		}
	}
	
	/* access second level table */
	if ((st[(v >> 12) & 0xff] & 3) != 0) {
		if (!(flags & KVMM_REPLACE)) {
			if (!(flags & KVMM_SKIP)) {
				kprintf("something already here; table:%x virtual:%x val:%x\n", st, v, st[(v>>12)&0xff]);
				VMMKCCEXIT(&vmm->lock);
				return 0;
			} else {
				/* just skip it with no error */
				VMMKCCEXIT(&vmm->lock);
				return 2;
			}
		}
	}
	//kprintf("   outside\n");
	/* update coarse table entries.. we used one (ONLY for kernel space) */
	if (ks->vmm.table == t) {
		ks->vmm_ucte -= 1;
	}

	//kprintf("mapped tbl:[%x] %x -> %x [flags:%x]\n", t, v, p, flags & 0xfff);
	
	flags = flags & 0xfff;
	st[(v >> 12) & 0xff] = p | flags | TLB_C_SMALLPAGE;
	
	VMMKCCEXIT(&vmm->lock);
	return 1;
}

int kvmm2_walkentries(KVMMTABLE *vmm, KVMM2_WALKCB cb) {
	uint32			*t;
	uint32			*st;
	uint32			x, y;
	uint32			c;
	
	VMMKCCENTER(&vmm->lock);
	
	t = vmm->table;
	
	for (c = 0, x = 0; x < 1024; ++x) {
		/* do not support anything other than coarse table */
		if ((t[x] & 0x3) > TLB_COARSE) {
			/* throw PANIC so we can catch this problem and solve it */
			PANIC("ONLY-COARSE-TABLE-SUPPORTED");
		}
		
		if ((t[x] & 0x3) == TLB_COARSE) {
			/* walk sub-table */
			st = (uint32*)(t[x] & ~0x3ff);
			/* free 1k page used by sub-table */
			for (y = 0; y < 256; ++y) {
				if ((st[y] & 3) != TLB_C_SMALLPAGE || (st[y] & 3) != 0) {
					PANIC("ONLY-4K-AND-ABSENT-SUPPORTED");
				}
				
				if ((st[y] & 3) == TLB_C_SMALLPAGE) {
					/* callback with page */
					cb((x << 20) | (y << 12), st[y] & ~0xfff);
					++c;
				}
			}
		}
	}
	
	VMMKCCEXIT(&vmm->lock);
	return c;
}

int kvmm2_unmap(KVMMTABLE *vmm, uintptr v, uint8 free) {
	uintptr				phy;
	uint32 volatile		*t, *st;
	KSTATE				*ks;
	
	VMMKCCENTER(&vmm->lock);
	
	ks = GETKS();
	
	t = vmm->table;
	
	if ((t[v >> 20] & 3) == 0) {	
		VMMKCCEXIT(&vmm->lock);
		return 0;
	}
	
	st = (uint32*)(t[v >> 20] & ~0x3ff);
	st = (uint32*)kvmm2_revget((uintptr)st, 0);
	
	if ((st[(v >> 12) & 0xff] & 3) == 0) {
		VMMKCCEXIT(&vmm->lock);
		return 0;
	}
	
	if (free) {
		/* free */
		phy = st[(v >> 12) & 0xff] & ~0xfff;
		k_heapBMFree(&ks->hphy, (void*)phy);
	}
	
	/* unmap */
	st[(v >> 12) & 0xff] = 0;
	VMMKCCEXIT(&vmm->lock);
	return 1;
}

int kvmm2_mapmulti(KVMMTABLE *vmm, uintptr v, uintptr p, uintptr c, uint32 flags) {
	uintptr			x;
	int				ret;
	
	VMMKCCENTER(&vmm->lock);
	
	for (x = 0; x < c; ++x) {
		kprintf("multi v:%x p:%x\n", v + 0x1000 * x, p + 0x1000 * x);
		ret = kvmm2_mapsingle(vmm, v + 0x1000 * x, p + 0x1000 * x, flags);  
		if (!ret) {
			PANIC("mapmulti-failed");
			VMMKCCEXIT(&vmm->lock);
			return 0;
		}
	}
	
	VMMKCCEXIT(&vmm->lock);
	return 1;
}

void ptwalker(uint32 *t) {
	uint32		x, y;
	uint32		*st;
	
	kprintf("ptwalker()..\n");
	for (x = 0; x < 4096; ++x) {
		if (t[x] != 0) {
			kprintf("  [i:%x p:%x]\n", x, t[x]);
			st = (uint32*)(t[x] & ~0x3ff);
			st = (uint32*)kvmm2_revget((uintptr)st, 0);
			kprintf("             [v:%x]\n", st);
			for (y = 0; y < 256; ++y) {
				if (st[y] != 0) {
					kprintf("      [%x]:%x\n", y, st[y]);
				}
			}
		}
	}
}

int kvmm2_init(KVMMTABLE *t) {
	uint32			x;
	KSTATE			*ks;
	uintptr			addr;
	 
	ks = GETKS();
	
	VMMKCCENTER(&t->lock);
	
	t->table = (uint32*)k_heapBMAllocBound(&ks->hphy, 4096 * 4, 14);
	
	if (!ks->vmm.table) {
		PANIC("vmm-table-alloc-failed");
		VMMKCCEXIT(&t->lock);
		return 0;
	}	
	
	if (!kvmm2_findregion(&ks->vmm, 4, 0, KMEMSIZE, 0, &addr)) {
		PANIC("findregionfailed");
		VMMKCCEXIT(&t->lock);
		return 0;
	}
	
	if (!kvmm2_mapmulti(&ks->vmm, addr, (uintptr)t->table, 4, TLB_C_AP_PRIVACCESS)) {
		PANIC("vmm-table-map-failed");
		VMMKCCEXIT(&t->lock);
		return 0;
	}
	
	t->table = (uint32*)(addr);
	
	uintptr			o;
	
	kvmm2_getphy(&ks->vmm, (uintptr)ks->vmm.table, &o);

	for (x = 0; x < 4096; ++x) {
		t->table[x] = 0;
	}
	
	/* unlock */
	VMMKCCEXIT(&t->lock);
	return 1;
}

int kvmm2_init_revtable() {
	uint32		x;
	KSTATE		*ks;
	
	ks = GETKS();

	ks->vmm_rev = (uint32*)k_heapBMAllocBound(&ks->hchk, 4096 * 4, 0);
	
	if (!ks->vmm_rev) {
		PANIC("vmm_rev.alloc.failed");
	}
	
	for (x = 0; x < 4096; ++x) {
		ks->vmm_rev[x] = 0;
	}
	
	return 1;
}

int kvmm2_baseinit() {
	KSTATE			*ks;
	uint32			x;
	char			buf[128];
	
	ks = GETKS();

	ks->vmm_ucte = 0;
	
	kprintf("CHECK:%x\n", ks);
	
	/* initialize kernel page table */
	ks->vmm.table = (uint32*)k_heapBMAllocBound(&ks->hphy, 4096 * 4, 14);
	
	if (!ks->vmm.table) {
		PANIC("FAILED-TO-ALLOC-TABLE");
	}
	
	for (x = 0; x < 4096; ++x) {
		ks->vmm.table[x] = 0;
	}
	
	/* initialize reverse map table */
	kvmm2_init_revtable();
	
	/* initial table stack*/
	kstack_init(&ks->tstack);
	
	/* map kernel page table (when we enable paging we still want to access it) */
	kprintf("ks->vmm.table:%x\n", ks->vmm.table);
	kvmm2_mapmulti(&ks->vmm, (uintptr)ks->vmm.table, (uintptr)ks->vmm.table, 4, TLB_C_AP_PRIVACCESS | KVMM_DIRECT);
}
