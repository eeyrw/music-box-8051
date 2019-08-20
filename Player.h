#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <stdint.h>
#include "SynthCore.h"


enum PLAY_STATUS{
    STATUS_STOP=0,
    STATUS_REDAY_TO_PLAY=1,
    STATUS_PLAYING=2
};

typedef struct _Player
{
	uint8_t status;
    uint32_t  currentTick;
	uint32_t  lastScoreTick;
	uint8_t  decayGenTick;
    uint8_t* scorePointer;
    Synthesizer mainSynthesizer;
} Player;


extern void PlayerInit(Player* player);
extern void Player32kProc(Player* player);
extern void PlayerProcess(Player* player);
extern void PlayerPlay(Player* player);

extern void UpdateTick(Player* player);
extern uint8_t PlayNoteTimingCheck(Player* player);
extern void PlayUpdateNextScoreTick(Player* player);
#endif