#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "SynthCore.h"
#include "Player.h"
#include "RegisterDefine.h"

__code extern unsigned char Score[];

__code ScoreListHeader* ScoreDataListPtr=(__code ScoreListHeader*)Score;

void ScoreDecodeProcess(Player *player)
{
    uint8_t temp;
    if (decayGenTick >= DECAY_TIME_FACTOR)
    {
        GenDecayEnvlopeAsm();
        decayGenTick = 0;
    }
    if (player->decoder.status == STATUS_DECODING)
    {
        uint32_t tmp;
        EA = 0;
        tmp = (currentTick >> 8);
        EA = 1;
        if (tmp >= player->decoder.lastScoreTick)
        {
            do
            {
                temp = *(player->decoder.scorePointer);
                player->decoder.scorePointer++;
                if (temp == 0xFF)
                    player->decoder.status = STATUS_STOP;
                else
                    NoteOnAsm(temp);
            } while ((temp & 0x80) == 0);

            UpdateNextScoreTick(&(player->decoder));
        }
    }
}

void PlaySchedulerNextScore(Player *player)
{
    player->scheduler.switchDirect = SCHEDULER_SCORE_NEXT;
    player->scheduler.status = SCHEDULER_SWITCHING;
    StopDecode(player);
}

void PlaySchedulerPreviousScore(Player *player)
{
    player->scheduler.switchDirect = SCHEDULER_SCORE_PREV;
    player->scheduler.status = SCHEDULER_SWITCHING;
    StopDecode(player);
}

__code uint8_t *GetScorePhyiscalAddr(uint32_t addr)
{
    return (__code uint8_t *)((__code uint8_t *)ScoreDataListPtr + addr);
}

void PlaySchedulerProcess(Player *player)
{
    __code uint32_t *ScoreList = (&(player->scheduler.scoreListHeader)->firstAddr);

    switch (player->scheduler.status)
    {
    case SCHEDULER_READY_TO_SWITCH:
        if (player->decoder.status == STATUS_STOP)
        {
            player->scheduler.switchDirect = SCHEDULER_SCORE_NEXT;
            player->scheduler.status = SCHEDULER_SWITCHING;
            StopDecode(player);
        }
        break;

    case SCHEDULER_SWITCHING:
        player->scheduler.status = player->scheduler.switchDirect;
        break;

    case SCHEDULER_SCORE_PREV:
        player->scheduler.currentScoreIndex--;
        if (player->scheduler.currentScoreIndex < 0)
            player->scheduler.currentScoreIndex = player->scheduler.maxScoreNum - 1;
        PlayScore(player, GetScorePhyiscalAddr(ScoreList[player->scheduler.currentScoreIndex]));
        player->scheduler.status = SCHEDULER_READY_TO_SWITCH;

        break;

    case SCHEDULER_SCORE_NEXT:
        player->scheduler.currentScoreIndex++;
        if (player->scheduler.currentScoreIndex >= player->scheduler.maxScoreNum)
        {
            player->scheduler.status = SCHEDULER_STOP;
            player->scheduler.currentScoreIndex = -1;
        }
        else
        {
            PlayScore(player, GetScorePhyiscalAddr(ScoreList[player->scheduler.currentScoreIndex]));
            player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
        }

        break;

    case SCHEDULER_STOP:
        /* Do nothing */
        break;
    default:
        break;
    }
}

void UpdateNextScoreTick(ScoreDecoder *decoder)
{
    uint32_t tempU32;
    uint8_t temp;
    tempU32 = decoder->lastScoreTick;
    do
    {
        temp = *(decoder->scorePointer);
        decoder->scorePointer++;
        tempU32 += temp;
    } while (temp == 0xFF);
    decoder->lastScoreTick = tempU32;
}

void StartPlayScheduler(Player *player)
{
    if (ScoreDataListPtr->identifer[0] == 'S' &&
        ScoreDataListPtr->identifer[1] == 'C' &&
        ScoreDataListPtr->identifer[2] == 'R' &&
        ScoreDataListPtr->identifer[3] == 'E')
    {
        player->scheduler.scoreListHeader = ScoreDataListPtr;
        player->scheduler.currentScoreIndex = -1;
        player->scheduler.maxScoreNum = (player->scheduler.scoreListHeader)->scoreCount;
        player->scheduler.schedulerMode = MODE_ORDER_PLAY;
        player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
    }
    else
    {
        player->scheduler.status = SCHEDULER_STOP;
    }
}

void StopPlayScheduler(Player *player)
{
    StopDecode(player);
    player->scheduler.status = SCHEDULER_STOP;
}

void SchedulerSetIntialRandomSeed(Player *player, uint8_t randomSeed)
{
    player->scheduler.currentScoreIndex = (randomSeed % player->scheduler.maxScoreNum) - 1;
}

void PlayScore(Player *player, __code uint8_t *score)
{
    // player->synthesizer.hwSet(SYNTH_HW_ON);
    currentTick = 0;
    player->decoder.lastScoreTick = 0;
    player->decoder.scorePointer = score;
    UpdateNextScoreTick(&(player->decoder));
    player->decoder.status = STATUS_DECODING;
}

void StopDecode(Player *player)
{
    // player->synthesizer.hwSet(SYNTH_HW_OFF);
    player->decoder.status = STATUS_STOP;
    currentTick = 0;
    player->decoder.lastScoreTick = 0;
}

void PlayerProcess(Player *player)
{
    ScoreDecodeProcess(player);
    PlaySchedulerProcess(player);
}

void PlayerInit(Player *player, Synthesizer *synthesizer)
{
    player->decoder.status = STATUS_STOP;
    currentTick = 0;
    player->decoder.lastScoreTick = 0;
    player->decoder.scorePointer = NULL;
    player->synthesizerPtr = synthesizer;
    SynthInit(player->synthesizerPtr);
}
