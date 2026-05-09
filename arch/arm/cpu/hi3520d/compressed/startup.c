/******************************************************************************
*    Copyright (c) 2009-2012 by Hisi.
*    All rights reserved.
* ***
*    Create by Czyong. 2012-04-27
*
******************************************************************************/

#include <config.h>
#include "startup.h"
/******************************************************************************/
#ifndef uart_early_puts
void uart_early_puts(const char *ss)
{
}
#endif

#ifndef reset_cpu
void reset_cpu(unsigned long addr)
{
}
#endif

/*
 * product/hiddrtv200/ddrtraining.c calls printf() inside its
 * ddrt_print_bit_result() debug path. UART isn't initialised at
 * this stage of the compressed wrapper, so provide a no-op stub —
 * matches the vendor prebuilt u-boot_hi3520d.bin (the format
 * strings are present in the binary but never reach stdout).
 */
int printf(const char *fmt, ...)
{
	return 0;
}
/******************************************************************************/
const unsigned int IMAGE_ENTRY = (TEXT_BASE + 0x100000);

typedef void (*ENTRY)(void);
/******************************************************************************/
#define error(_s)               uart_early_puts(_s)
#define putstr(_s)              uart_early_puts(_s)

#define large_malloc(_n)        malloc(_n)
#define large_free(_n)          free(_n)

/******************************************************************************/
/*
 * this value will be (_armboot_start - CONFIG_SYS_MALLOC_LEN)
 */
static char *malloc_start;

/******************************************************************************/
static void *malloc(unsigned int size)
{
	void *ptr = malloc_start;
	malloc_start += size;
	return ptr;
}
/******************************************************************************/
static void free(void *ptr)
{
}
/******************************************************************************/

#include "unlzma.c"

/******************************************************************************/
void start_armboot(void)
{
	ENTRY entry = (ENTRY)IMAGE_ENTRY;
	unsigned char *pdst = (unsigned char *)IMAGE_ENTRY;
	unsigned int image_data_len = input_data_end - input_data;

	malloc_start = (char *)(_armboot_start - CONFIG_SYS_MALLOC_LEN);

	/* DDR should larger than 16M */
	enable_mmu();

	dcache_enable(0);

	if (input_data[0] == 0x5D) {
		decompress(input_data, image_data_len, pdst);
	} else {
		int *s = (int *)input_data;
		int *d = (int *)pdst;
		unsigned int len = ((image_data_len + 3) >> 2);
		while (len--)
			*d++ = *s++;
	}
	dcache_disable();

	stop_mmu();

	entry();
}

/******************************************************************************/
void do_undefined_instruction(void)
{
	reset_cpu(0);
}

void do_software_interrupt(void)
{
	reset_cpu(0);
}

void do_prefetch_abort(void)
{
	reset_cpu(0);
}

void do_data_abort(void)
{
	reset_cpu(0);
}

void do_not_used(void)
{
	reset_cpu(0);
}

void do_fiq(void)
{
	reset_cpu(0);
}

void do_irq(void)
{
	reset_cpu(0);
}
