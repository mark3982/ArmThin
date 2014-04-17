#include "corelib/core.h"

void filler() {
	uint32			*x;
	
	x = (uint32*)0;
	
	x[0] = 'a';
	x[1] = 'b';
}

int main() {
	int		x;
	int		y;
	unsigned int tps;
	unsigned int 	*smmio;
	
	smmio = (unsigned int*)0xa0000000;
	
	smmio[0] = '+';
	
	tps = getTicksPerSecond();
	
	for (;;) {
		printf("FS BEFORE SLEEP\n");
		sleep(10);
		printf("FS RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'A';
		smmio[0] = '\n';
	}
	return 0;
}