#include "linklist.h"

/*
	@sdescription:		Adds item to a linked list.
*/
void ll_add(void **p, void *i) {
	LL		*_i;
	LL		*croot;
		
	_i = (LL*)i;
	
	_i->next = *p;
	if (p && *p) {
		croot = ((LL*)(*p));
		_i->prev = croot->prev;
		croot->prev = _i;
	} else {
		_i->prev = 0;
	}
	
	if (p) {
		*p = _i;
	}
}

/*
	@sdescription:			Removes an item from a linked list.
*/
void ll_rem(void **p, void *i) {
	LL			*_i;
	
	_i = (LL*)i;

	if (_i->prev) {
		_i->prev->next = _i->next;
	}
	
	if (_i->next) {
		_i->next->prev = _i->prev;
	}
	
	if (p) {
		if (*p == i) {
			if (_i->prev) {
				*p = _i->prev;
			} else {
				*p = _i->next;
			}
		}
	}
}
