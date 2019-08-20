#include <stdint.h>
#include <stdio.h>
#include <string.h>
/*
 * Redirect stdout to UART
 */
int putchar(int c)
{
	//uart_write(c);
	return 0;
}


/*
 * Redirect stdin to UART
 */
int getchar()
{
	//return uart_read();
}
