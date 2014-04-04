#ifndef KMOD_H
#define KMOD_H
#include "stdtypes.h"

typedef struct _KATTMOD {
	uint32			size;
	uint32			signatureA;
	uint32			signatureB;
	uint32			flags;
	uint32			ptr[];
} KATTMOD;

KATTMOD *kPkgGetNextMod(KATTMOD *mod);
KATTMOD *kPkgGetFirstMod();
uintptr kPkgGetTotalLength();
#endif