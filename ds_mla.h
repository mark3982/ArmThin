/*
	Data Structure
	
	Multiple Limited Arrays
*/
#ifndef H_DS_MLA
#define H_DS_MLA
#include "stdtypes.h"

typedef struct _MLAB {
	struct _MLAB		*next;
	struct _MLAB		*prev;
	uintptr				domain;
	uint32				max;
	uint32				used;
	uintptr				slots[];
} MLAB;

typedef struct _MLA {
	MLAB			*blocks;
	uint32			dmax;
} MLA;

int mla_init(MLA *sb, uint32 dmax);
int mla_get(MLA *sb, uintptr *domain, uintptr *out);
int mla_add(MLA *sb, uintptr domain, uintptr signal);
#endif