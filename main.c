#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "Bsp.h"

extern void TestProcess(void);
extern __code unsigned char Score[];
extern __data Player mainPlayer;

void VisualizeSound(void)
{
	int16_t t;
	t = synthForAsm.mixOut;
	if (t < 0)
		t = -t;
	t = t << 1;
	PWMA_CCR4H = (t >> 8) & 0xff;
	PWMA_CCR4L = t & 0xff;
}
void main()
{

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
		VisualizeSound();
	}
}
