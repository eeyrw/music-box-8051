#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "SynthCore.h"
#include "Player.h"
#include "RegisterDefine.h"

void PlayUpdateNextScoreTickP(Player *player)
{
    uint32_t tempU32 = player->lastScoreTick;
    uint8_t temp;
    do
    {
        temp = *(player->scorePointer);
        player->scorePointer++;
        tempU32 += temp;
    } while (temp == 0xFF);
    player->lastScoreTick = tempU32;
}

void PlayerProcess(Player *player)
{

    uint8_t temp;
    if (decayGenTick >= DECAY_TIME_FACTOR)
    {
        GenDecayEnvlopeAsm();
        decayGenTick = 0;
    }
    if (player->status == STATUS_PLAYING)
    {
        uint32_t t;
        EA = 0;
        t = (currentTick >> 8);
        EA = 1;
        if (t >= player->lastScoreTick) //if (PlayNoteTimingCheck(player))
        {
            do
            {
                temp = *(player->scorePointer);
                player->scorePointer++;
                if (temp == 0xFF)
                {
                    player->status = STATUS_STOP;
                }
                else
                {
                    NoteOnAsm(temp);
                    putchar(temp);
                }
            } while ((temp & 0x80) == 0);
            if (player->status == STATUS_PLAYING)
                PlayUpdateNextScoreTickP(player);
        }
    }
}

void PlayerPlay(Player *player, uint8_t *score)
{
    player->lastScoreTick = 0;
    player->scorePointer = score;
    currentTick = 0;
    PlayUpdateNextScoreTickP(player);
    player->status = STATUS_PLAYING;
}

void PlayerInit(Player *player, Synthesizer *synthesizer)
{
    player->status = STATUS_STOP;
    player->lastScoreTick = 0;
    currentTick = 0;
    player->scorePointer = NULL;
    player->synthesizerPointer = synthesizer;
    SynthInit(synthesizer);
}