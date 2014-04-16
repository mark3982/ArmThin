#ifndef ARMOS_KH_KHEAP
#define ARMOS_KH_KHEAP
#include "stdtypes.h"

//#define K_HEAP_TYPE_LINKEDCHUNKSANDBLOCKS
#ifdef K_HEAP_TYPE_LINKEDCHUNKSANDBLOCKS
#define k_heapInit k_heapLCABInit
#define k_heapAddBlock k_heapLCABAddBlock
#define k_heapFree k_heapLCABFree
#define k_heapAlloc k_heapLCABAlloc
#define KHEAP KHEAPLCAB
#define KHEAPFLAG_USED			0x80000000
#endif

#define K_HEAP_TYPE_BITMAPBLOCKS
#ifdef K_HEAP_TYPE_BITMAPBLOCKS
#define k_heapInit k_heapBMInit
#define k_heapAddBlock k_heapBMAddBlock
#define k_heapFree k_heapBMFree
#define k_heapAlloc k_heapBMAlloc
#define KHEAP KHEAPBM
#endif

typedef struct _KHEAPHDRLCAB {
	uint32				flagsize;
} KHEAPHDRLCAB;

typedef struct _KHEAPBLOCKLCAB {
	uint32					size;
	uint32					used;
	struct _KHEAPBLOCKLCAB	*next;
} KHEAPBLOCKLCAB;

typedef struct _KHEAPLCAB {
	KHEAPBLOCKLCAB			*fblock;
} KHEAPLCAB;

typedef struct _KHEAPBLOCKBM {
	struct _KHEAPBLOCKBM	*next;
	uint32					size;
	uint32					used;
	uint32					bsize;
	uint32					lfb;
	uintptr					data;
	uint8					*bm;
} KHEAPBLOCKBM;

typedef struct _KHEAPBM {
	KHEAPBLOCKBM			*fblock;
} KHEAPBM;

typedef struct _KHEAPBLOCKSS {
	struct _KHEAPBLOCKSS	*next;
	uint32					top;
	uint32					max;
	uintptr					size;			/* total size in bytes including this header */
} KHEAPBLOCKSS;

typedef struct _KHEAPSS {
	KHEAPBLOCKSS			*fblock;
	uint32					bsize;
} KHEAPSS;

void k_heapSSInit(KHEAPSS *heap, uint32 bsize);
int k_heapSSAddBlock(KHEAPSS *heap, uintptr addr, uint32 size);
void *k_heapSSAlloc(KHEAPSS *heap, uint32 size);
void k_heapSSFree(KHEAPSS *heap, void *ptr);

void k_heapBMInit(KHEAPBM *heap);
int k_heapBMAddBlock(KHEAPBM *heap, uintptr addr, uint32 size, uint32 bsize);
int k_heapBMAddBlockEx(KHEAPBM *heap, uintptr addr, uint32 size, uint32 bsize, KHEAPBLOCKBM *b, uint8 *bm, uint8 isBMInside);
void *k_heapBMAlloc(KHEAPBM *heap, uint32 size);
int k_heapBMFree(KHEAPBM *heap, void *ptr);
uintptr k_heapBMGetBMSize(uintptr size, uintptr bsize);
void *k_heapBMAllocBound(KHEAPBM *heap, uint32 size, uint32 mask);
void k_heapBMSet(KHEAPBM *heap, uintptr ptr, uintptr size, uint8 rval);

void k_heapLCABInit(KHEAPLCAB *heap);
int k_heapLCABAddBlock(KHEAPLCAB *heap, uintptr addr, uint32 size);
void k_heapLCABFree(KHEAPLCAB *heap, void *ptr);
void* k_heapLCABAlloc(KHEAPLCAB *heap, uint32 size);
#endif
