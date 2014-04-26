#include "kmod.h"

extern uint8 _EOI;

/*
	Get the next module after the specified module.
*/
KATTMOD *kPkgGetNextMod(KATTMOD *mod) {
	KATTMOD		*n;
	
	n = (KATTMOD*)((uintptr)mod + mod->size + sizeof(KATTMOD));
	
	/* make sure there actually exists another module after this one */
	if (n->signatureA != 0x12345678 || ~n->signatureA != n->signatureB) {
		kprintf("next mod not found\n");
		return 0;
	}
	
	return n;
}

uintptr kPkgGetTotalLength() {
	KATTMOD			*m, *lm;
	
	lm = 0;
	for (m = kPkgGetFirstMod(); m; m = kPkgGetNextMod(m)) {
		lm = m;
	}
	
	if (!lm) {
		return (uintptr)&_EOI;
	}
	
	return lm->size + (uintptr)lm + sizeof(KATTMOD) + 1;
}

/*
	Locate first module, if any exists, at end of our image.
*/
KATTMOD *kPkgGetFirstMod() {
	uint32		*p;
	uint32		x;
	uintptr		_p;
	
	_p = (uintptr)&_EOI - 4 * 20;
	_p = _p & ~0x3;
	
	/* find signature of first module */
	for (p = (uint32*)_p, x = 0; x < 256; ++x) {
		/*
			This is a very tricky situation. We do not want the compiler
			to emit these sequence of bytes neither through an immediate
			operand or through optimization.
		*/
		if (p[x] == 0x12345678 && p[x + 1] == ~p[x]) {
			/* backup 32-bits (4 bytes) so we grab the size placed by attachmod.py */
			return (KATTMOD*)&p[x - 1];
		}
	}
	
	return 0;
}
