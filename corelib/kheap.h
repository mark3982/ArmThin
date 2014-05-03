#ifndef ARMOS_KH_KHEAP
#define ARMOS_KH_KHEAP
#include "stdtypes.h"
#ifdef KERNEL
#include "../atomic.h"
#endif

/*
	@sdescription: 		Since this code is shared between kernel and user space
						these macros help do the different kinds of locking needed.
	@ldescription:		At the moment these are just simply omitted for userspace
						but I may come back do locking for multiple threads later.
*/
#ifndef KCCENTER
#define KCCENTER(x)
#endif
#ifndef KCCEXIT
#define KCCEXIT(x)
#endif

/* @sdescription: Represents each block added to the heap. */
typedef struct _KHEAPBLOCKBM {
	struct _KHEAPBLOCKBM	*next;
	uint32					size;
	uint32					used;
	uint32					bsize;
	uint32					lfb;
	uintptr					data;
	uint8					*bm;
} KHEAPBLOCKBM;

/* @sdescription: Represents a heap. */
typedef struct _KHEAPBM {
	KHEAPBLOCKBM				*fblock;
	#ifdef KERNEL
	KATOMIC_CCLOCK volatile		lock;
	#endif
} KHEAPBM;

void k_heapBMInit(KHEAPBM *heap);
int k_heapBMAddBlock(KHEAPBM *heap, uintptr addr, uint32 size, uint32 bsize);
int k_heapBMAddBlockEx(KHEAPBM *heap, uintptr addr, uint32 size, uint32 bsize, KHEAPBLOCKBM *b, uint8 *bm, uint8 isBMInside);
void *k_heapBMAlloc(KHEAPBM *heap, uint32 size);
int k_heapBMFree(KHEAPBM *heap, void *ptr);
uintptr k_heapBMGetBMSize(uintptr size, uintptr bsize);
void *k_heapBMAllocBound(KHEAPBM *heap, uint32 size, uint32 mask);
void k_heapBMSet(KHEAPBM *heap, uintptr ptr, uintptr size, uint8 rval);
#endif
