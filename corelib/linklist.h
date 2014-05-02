#ifndef CORELIB_H_LINKLIST
#define CORELIB_H_LINKLIST
typedef struct _LL {
	struct _LL			*next;
	struct _LL			*prev;
} LL;

void ll_add(void **p, void *i);
void ll_rem(void **p, void *i);
#endif