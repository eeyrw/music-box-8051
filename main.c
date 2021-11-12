#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "Bsp.h"

extern void TestProcess(void);
extern __code unsigned char Score[];
extern __data Player mainPlayer;

int16_t abs(int16_t n)
{
	if(n<0)
		return -n;
	else
		return n;
}
void main()
{
	int16_t t;
	PlayerInit(&mainPlayer, &synthForAsm);
	HardwareInit();
#ifndef RUN_TEST
	PlayerPlay(&mainPlayer, Score);
	StartAudioOutput();
#else
	TestProcess();
#endif
	while (1)
	{
		PlayerProcess(&mainPlayer);
		t = abs(synthForAsm.mixOut)<<1;
		PWMA_CCR4H=(t>>8)&0xff;
		PWMA_CCR4L=t&0xff;
	}
}
