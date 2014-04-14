#ifndef VMM_H
#define VMM_H
#include "stdtypes.h"
#include "main.h"

typedef struct _KVMMTABLE {
	uint32			*table;
} KVMMTABLE;

typedef struct _KSTACKBLOCK {
	struct _KSTACKBLOCK			*prev;
	struct _KSTACKBLOCK			*next;
	uint32						max;
	uint32						top;
} KSTACKBLOCK;

typedef struct _KSTACK {
	KSTACKBLOCK					*cur;
} KSTACK;

/* first level table */
#define TLB_FAULT			0x000		/* entry is unmapped (bits 32:2 can be used for anything) but access generates an ABORT */
#define TLB_SECTION			0x002		/* entry maps 1MB chunk */
#define TLB_COARSE			0x001		/* sub-table */
#define TLB_DOM_NOACCESS	0x00		/* generates fault on access */
#define TLB_DOM_CLIENT		0x01		/* checked against permission bits in TLB entry */
#define TLB_DOM_RESERVED	0x02		/* reserved */
#define TLB_DOM_MANAGER		0x03		/* no permissions */
#define TLB_STDFLAGS		0xc00		/* normal flags */
/* second level coarse table */
#define TLB_C_LARGEPAGE		0x1			/* 64KB page */
#define TLB_C_SMALLPAGE		0x2			/* 4KB page */
/* AP (access permission) flags for coarse table [see page 731 in ARM_ARM] */
#define TLB_C_AP_NOACCESS	(0x00<<4)	/* no access */
#define TLB_C_AP_PRIVACCESS	(0x01<<4)	/* only system access  RWXX */
#define TLB_C_AP_UREADONLY	(0x02<<4)	/* user read only  RWRX */
#define TLB_C_AP_FULLACCESS	(0x03<<4)	/* RWRW */	
/* AP (access permission) flags [see page 709 in ARM_ARM; more listed] */
#define TLB_AP_NOACCESS		(0x00<<10)	/* no access */
#define TLB_AP_PRIVACCESS	(0x01<<10)	/* only system access  RWXX */
#define TLB_AP_UREADONLY	(0x02<<10)	/* user read only  RWRX */
#define TLB_AP_FULLACCESS	(0x03<<10)	/* RWRW */

#define TLB_DOM(x)			(x<<5)		/* domain value (16 unique values) */
#define DOM_TLB(x,y)		(x<<(y*2))	/* helper macro for setting appropriate bits in domain register */

#define KVMM_DIRECT			0x80000000
#define KVMM_SKIP			0x40000000
#define KVMM_REPLACE		0x20000000

int kvmm2_baseinit(uintptr lowaddr);
int kvmm2_init_revtable();
int kvmm2_init(KVMMTABLE *t);
int kvmm2_mapmulti(KVMMTABLE *vmm, uintptr v, uintptr p, uintptr c, uint32 flags);
int kvmm2_unmap(KVMMTABLE *vmm, uintptr v, uint8 free);
int kvmm2_mapsingle(KVMMTABLE *vmm, uintptr v, uintptr p, uint32 flags);
int kvmm2_allocregionat(KVMMTABLE *vmm, uintptr pcnt, uintptr start, uint32 flags);
int kvmm2_allocregion(KVMMTABLE *vmm, uintptr pcnt, uintptr low, uintptr high, uint32 flags, uintptr *out);
int kvmm2_findregion(KVMMTABLE *vmm, uintptr tc, uintptr low, uintptr high, uint32 flags, uintptr *out);
int kvmm2_getphy(KVMMTABLE *vmm, uintptr v, uintptr *o);
int kvmm2_get1Ktable(uintptr *o, uint32 flags);
uint32 kvmm2_getucts(KVMMTABLE *vmm, uint32 *slot);
int kvmm2_getu4k(KVMMTABLE *vmm, uintptr *o, uint32 flags);
uintptr kvmm2_revget(uintptr p, uint8 opt);
int kvmm2_revset(uintptr p, uint32 v, uint8 opt);
uintptr kvmm2_rndup(uintptr sz);
/*
	These are used for tracking reference counts on physical memory pages. 
	This allows me to share memory and hand it back to the physical page
	manager when it is no longer referenced.
*/
uint32 kvmm2_revdec(uintptr v);
uint32 kvmm2_revinc(uintptr v);

int kstack_init(KSTACK *stack, uintptr lowaddr);
int kstack_push(KSTACK *stack, uint32 v);
int kstack_pop(KSTACK *stack, uint32 *v);
int kstack_empty(KSTACK *stack);
int kstack_initblock(KSTACKBLOCK *b);

void ptwalker(uint32 *t);
#endif