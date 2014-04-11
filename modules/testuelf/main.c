#include "corelib/core.h"

int main() {
	int				x;
	int				y;
	unsigned int	tps;
	unsigned int 	*smmio;
	
	smmio = (unsigned int*)0xa0000000;
	
	printf("TESTUELF START\n");
	
	tps = getTicksPerSecond();
	
	for (;;) {
		printf("TESTUELF BEFORE SLEEP\n");
		sleep(1);
		printf("TESTUELF RANGE %x\n", ((uint8*)0x80000400)[0]);
		smmio[0] = 'B';
	}
	return 0;
}