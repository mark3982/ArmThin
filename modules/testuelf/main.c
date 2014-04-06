int _start(unsigned int *smmio) {
	int		x;
	int		y;
	int		*a;
	
	for (;;) {
		for (x = 0; x < 0xfffff; ++x);
		smmio[0] = 'G';
		smmio[0] = 'K';
		
		++y;
		if (y > 100) {
			a = (int*)0x0;
			a[0] = 33;
		}
	}
	return 0;
}