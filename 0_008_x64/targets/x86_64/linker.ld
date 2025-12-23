ENTRY(start)
OUTPUT_FORMAT(elf64-x86-64)

PHDRS

{

    kernel_load PT_LOAD AT(0x100000) FLAGS(7); /* RWE */

}



SECTIONS

{

	. = 0x100000; /* Set the base virtual and physical address for the kernel */



	.text : ALIGN(4K)

	{

		KEEP(*(.multiboot_header))

		*(.multiboot)

		*(.text*)

		*(.rodata)

		*(.eh_frame)

	} : kernel_load



	.data : ALIGN(4K)

	{

		start_ctors = .;

		KEEP(*(.init_array));

		KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)));

		end_ctors = .;

		

		*(.data*)

	} : kernel_load



		.bss : ALIGN(4K)



		{



			*(.bss*)



		} : kernel_load

	/DISCARD/ :
	{
		*(COMMON)
		*(.comment)
		*(.fini_array*)
	}
}
