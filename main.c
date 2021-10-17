#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "Bsp.h"

extern void TestProcess(void);
extern __code unsigned char Score[];
extern __data Player mainPlayer;

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
	}
}
