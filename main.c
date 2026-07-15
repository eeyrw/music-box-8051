#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "Bsp.h"
#include "Protocol.h"
#include "Storage.h"

extern void TestProcess(void);
MEM_XDATA(Player) mainPlayer;

void main()
{
	PlayerInit(&mainPlayer, &synthForAsm);
	HardwareInit();
	AdsrInit();
	Proto_Init();

#ifndef RUN_TEST
	storage_auto_detect();
	StartPlayScheduler(&mainPlayer, MODE_LIST_ONCE);

	SynthDitherInit(&synthForAsm, GetRandom() | ((uint16_t)GetRandom() << 8));
	SchedulerPlaySong(&mainPlayer, GetRandom() % 5);
	StartAudioOutput();
#else
	TestProcess();
#endif

	while (1)
	{
		SynthProcess();
		PlayerProcess(&mainPlayer);
		Proto_Process();
	}
}
