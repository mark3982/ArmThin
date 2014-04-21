ENTRY (entry)

SECTIONS
{
	/*
	. = 0x0;
	.bss : { *(.bss*) }
	*/
	
    . = 0x10000;
	_BOI = .;
    .text : { 
				*(.boot*)
				*(.text*) *(.rodata*) *(.data*) *(.bss*)
	}
	_EOI = .;
}