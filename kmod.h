#ifndef KMOD_H
#define KMOD_H
#include "stdtypes.h"

typedef struct _KATTMOD {
	uint32			size;
	uint32			signatureA;
	uint32			signatureB;
	uint32			type;
	uint32			slot[];
} KATTMOD;

KATTMOD *kPkgGetNextMod(KATTMOD *mod);
KATTMOD *kPkgGetFirstMod();
uintptr kPkgGetTotalLength();
#endif