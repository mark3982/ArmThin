#include "corelib/core.h"

int main() {
	int		x;
	int		y;
	unsigned int tps;
	unsigned int 	*smmio;
	
	smmio = (unsigned int*)0xa0000000;
	
	tps = getTicksPerSecond();
	
	for (;;) {
		printf("FS BEFORE SLEEP\n");
		sleep(2);
		printf("FS RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'A';
	}
	return 0;
}