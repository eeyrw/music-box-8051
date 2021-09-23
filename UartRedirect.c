#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "STC15F_SDCC.h"
/*
 * Redirect stdout to UART
 */
int putchar(int c)
{
	SBUF = (uint8_t)c;
	while (!TI)
		;
	TI = 0;
	return 0;
}

/*
 * Redirect stdin to UART
 */
int getchar()
{
	while (!RI)
		;
	RI = 0;
	return (int)SBUF;
}
