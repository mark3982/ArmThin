int _start(unsigned int *smmio) {
	int		x;
	int		y;
	int		*a;
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		smmio[0] = 'A';
		
		asm("	mov r0, #0xffffff \n\
				swi #101");
	}
	return 0;
}