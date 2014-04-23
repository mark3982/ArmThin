#include "kheap.h"

void k_heapBMInit(KHEAPBM *heap) {
	heap->fblock = 0;
}

int k_heapBMAddBlock(KHEAPBM *heap, uintptr addr, uint32 size, uint32 bsize) {
	KHEAPBLOCKBM			*b;
	uintptr					bmsz;
	uint8					*bm;

	b = (KHEAPBLOCKBM*)addr;
	bmsz = k_heapBMGetBMSize(size, bsize);
	bm = (uint8*)(addr + sizeof(KHEAPBLOCKBM));
	/* important to set isBMInside... (last argument) */
	return k_heapBMAddBlockEx(heap, addr + sizeof(KHEAPBLOCKBM), size - sizeof(KHEAPBLOCKBM), bsize, b, bm, 1);
}

uintptr k_heapBMGetBMSize(uintptr size, uintptr bsize) {
	return size / bsize;
}

int k_heapBMAddBlockEx(KHEAPBM *heap, uintptr addr, uint32 size, uint32 bsize, KHEAPBLOCKBM *b, uint8 *bm, uint8 isBMInside) {
	uint32				bcnt;
	uint32				x;
	
	b->size = size;
	b->bsize = bsize;
	b->data = addr;
	b->bm = bm;
	
	b->next = heap->fblock;
	heap->fblock = b;

	bcnt = size / bsize;
	
	kprintf("bcnt:%x bm:%x size:%x bsize:%x\n", bcnt, bm, size, bsize);
	/* clear bitmap */
	for (x = 0; x < bcnt; ++x) {
		bm[x] = 0;
		//kprintf("&bm[x]:%x x:%x bcnt:%x\n", &bm[x], x, bcnt);
	}

	bcnt = (bcnt / bsize) * bsize < bcnt ? bcnt / bsize + 1 : bcnt / bsize;
	
	/* if BM is not inside leave this space avalible */
	if (isBMInside) {
		/* reserve room for bitmap */
		for (x = 0; x < bcnt; ++x) {
			bm[x] = 5;
		}
	}
	
	b->lfb = bcnt - 1;
	
	b->used = bcnt;
	kprintf("HEREZ bcnt:%x b->used:%x b->lfb:%x\n", bcnt, b->used, b->lfb);
	return 1;
}

static uint8 k_heapBMGetNID(uint8 a, uint8 b) {
	uint8		c;	
	for (c = a + 1; c == b || c == 0; ++c);
	return c;
}

void *k_heapBMAlloc(KHEAPBM *heap, uint32 size) {
	return k_heapBMAllocBound(heap, size, 0);
}

void *k_heapBMAllocBound(KHEAPBM *heap, uint32 size, uint32 bound) {
	KHEAPBLOCKBM		*b;
	uint8				*bm;
	uint32				bcnt;
	uint32				x, y, z;
	uint32				bneed;
	uint8				nid;
	uint32				max;
	
	bound = ~(~0 << bound);
	/* iterate blocks */
	for (b = heap->fblock; b; b = b->next) {
		/* check if block has enough room */\
		kprintf("bmalloc:%x; b:%x b->size:%x b->used:%x size:%x\n", heap, b, b->size, b->used * b->bsize, size);
		if (b->size - (b->used * b->bsize) >= size) {
			bcnt = b->size / b->bsize;		
			bneed = (size / b->bsize) * b->bsize < size ? size / b->bsize + 1 : size / b->bsize;
			bm = (uint8*)b->bm;
			for (x = (b->lfb + 1 >= bcnt ? 0 : b->lfb + 1); x != b->lfb; ++x) {
				/* just wrap around */
				if (x >= bcnt) {
					x = 0;
				}		

				/*
					this is used to allocate on specified boundaries larger than the block size
				*/
				if ((((x * b->bsize) + b->data) & bound) != 0)
					continue;	
					
				if (bm[x] == 0) {	
					/* count free blocks */
					max = bcnt - x;
					for (y = 0; bm[x + y] == 0 && y < bneed && y < max; ++y);
					
					/* we have enough, now allocate them */
					if (y == bneed) {
						/* find ID that does not match left or right */
						nid = k_heapBMGetNID(bm[x - 1], bm[x + y]);
						
						/* allocate by setting id */
						for (z = 0; z < y; ++z) {
							bm[x + z] = nid;
						}
						/* optimization */
						b->lfb = (x + bneed) - 2;

						/* count used blocks NOT bytes */
						b->used += y;
						return (void*)((x * b->bsize) + b->data);
					}
					
					/* x will be incremented by one ONCE more in our FOR loop */
					x += (y - 1);
					continue;
				}
			}
		}
	}
	
	return 0;
}

void k_heapBMSet(KHEAPBM *heap, uintptr ptr, uintptr size, uint8 rval) {
	KHEAPBLOCKBM		*b;	
	uintptr				ptroff, endoff;
	uint32				bi, x, ei;
	uint8				*bm;
	uint8				id;
	uint32				max;
	
	for (b = heap->fblock; b; b = b->next) {
		/* check if region effects block */
		if (
			/* head end resides inside block */
			(ptr >= b->data && ptr < b->data + b->size) ||
			/* tail end resides inside block */
			((ptr + size) >= b->data && (ptr + size) < b->data + b->size) ||
			/* spans across but does not start or end in block */
			(ptr < b->data && (ptr + size) > b->data + b->size)
		) {
			/* found block */
			if (ptr >= b->data) {
				ptroff = ptr - b->data;  /* get offset to get block */
				/* block offset in BM */
				bi = ptroff / b->bsize;
			} else {
				/* do not start negative on bitmap */
				bi = 0;
			}
			
			/* access bitmap pointer in local variable */
			bm = b->bm;

			ptr = ptr + size;
			endoff = ptr - b->data;
			
			/* end index inside bitmap */
			ei = (endoff / b->bsize) * b->bsize < endoff ? (endoff / b->bsize) + 1 : endoff / b->bsize;
			++ei;
			
			/* region could span past end of a block so adjust */
			max = b->size / b->bsize;
			max = ei > max ? max : ei;
			
			/* set bitmap buckets */
			for (x = bi; x < max; ++x) {
				bm[x] = rval;
			}
			
			/* update free block count */
			if (rval == 0) {
				b->used -= ei - bi;
			} else {
				b->used += ei - bi;
			}
			
			/* do not return as region could span multiple blocks.. so check the rest */
		}
	}
	
	/* this error needs to be raised or reported somehow */
	return;
}

int k_heapBMFree(KHEAPBM *heap, void *ptr) {
	KHEAPBLOCKBM		*b;	
	uintptr				ptroff;
	uint32				bi, x;
	uint8				*bm;
	uint8				id;
	uint32				max;
	
	for (b = heap->fblock; b; b = b->next) {
		if ((uintptr)ptr > b->data && (uintptr)ptr < b->data + b->size) {
			/* found block */
			ptroff = (uintptr)ptr - b->data;  /* get offset to get block */
			/* block offset in BM */
			bi = ptroff / b->bsize;
			/* .. */
			bm = b->bm;
			/* clear allocation */
			id = bm[bi];

			max = b->size / b->bsize;
			for (x = bi; bm[x] == id && x < max; ++x) {
				bm[x] = 0;
			}
			/* update free block count */
			b->used -= x - bi;
			return 1;
		}
	}
	
	/* this error needs to be raised or reported somehow */
	return 0;
}