#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "Bsp.h"

extern void TestProcess(void);
Player mainPlayer;

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
	// 选择播放模式:
	//   MODE_ORDER_PLAY  — 循环无限播放
	//   MODE_LIST_ONCE   — 全部曲目播完一轮停止
	//   MODE_SINGLE_SONG — 只播一首停止 (UART 0xFE/0xFD 可手动切歌)
	StartPlayScheduler(&mainPlayer, MODE_LIST_ONCE);

	// 随机选起始曲目: (ADC噪声 % 曲目总数) — 1, 切歌时自动 +1
	SchedulerPlaySong(&mainPlayer, GetRandom() % 5);
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
