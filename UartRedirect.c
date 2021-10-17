#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "RegisterDefine.h"
/*
 * Redirect stdout to UART
 */
int putchar(int c)
{
	if (c == '\n')
	{
		SBUF = '\r';
		while (!TI)
			;
		TI = 0;
		SBUF = '\n';
		while (!TI)
			;
		TI = 0;
	}
	else
	{
		SBUF = c;
		while (!TI)
			;
		TI = 0;
	}

	return c;
}

/*
 * Redirect stdin to UART
 */
int getchar()
{
	//while (!RI)
	//	;
	//RI = 0;
	return SBUF;
}
