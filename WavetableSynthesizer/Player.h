#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <stdint.h>
#include "SynthCore.h"
#include "PeriodTimer.h"

enum DECODER_STATUS
{
    STATUS_STOP = 0,
    STATUS_REDAY_TO_DECODE = 1,
    STATUS_DECODING = 2
};

enum SCHEDULER_MODE
{
    MODE_ORDER_PLAY = 0,
};

enum SCHEDULER_STATUS
{
    SCHEDULER_READY_TO_SWITCH = 0,
    SCHEDULER_SWITCHING,
    SCHEDULER_SCORE_PREV,
    SCHEDULER_SCORE_NEXT,
    SCHEDULER_STOP,
};

typedef struct _ScoreListHeader
{
    char identifer[4];
    uint32_t scoreCount;
    uint32_t firstAddr;
} ScoreListHeader;

typedef struct _ScoreDecoder
{
    uint32_t lastScoreTick;
    uint32_t status;
    __code uint8_t *scorePointer;
} ScoreDecoder;

typedef struct _PlayScheduler
{
    uint32_t schedulerMode;
    int32_t currentScoreIndex;
    uint32_t maxScoreNum;
    __code ScoreListHeader *scoreListHeader;
    uint32_t status;
    uint32_t switchDirect;
} PlayScheduler;

typedef struct _Player
{
    ScoreDecoder decoder;
    PlayScheduler scheduler;
} Player;


extern void PlayerInit(Player *player, Synthesizer *synthesizer);
extern void Player32kProc(Player *player);
extern void PlayerProcess(Player *player);
extern void ScoreDecodeProcess(Player *player);
extern void PlayScore(Player *player, __code uint8_t *score);
extern void StopDecode(Player *player);
extern void StartPlayScheduler(Player *player);
extern void StopPlayScheduler(Player *player);
extern void PlaySchedulerNextScore(Player *player);
extern void PlaySchedulerPreviousScore(Player *player);
extern void SchedulerSetIntialRandomSeed(Player *player, uint8_t randomSeed);
extern void UpdateTick(ScoreDecoder *decoder);
extern void UpdateNextScoreTick(ScoreDecoder *decoder);

#endif