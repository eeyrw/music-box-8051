#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "SynthCore.h"
#include "Player.h"


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
        if (PlayNoteTimingCheck(player))
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
                }
            } while ((temp & 0x80) == 0);
            PlayUpdateNextScoreTick(player);
        }
    }
}

void PlayerPlay(Player *player, uint8_t *score)
{
    player->lastScoreTick = 0;
    player->scorePointer = score;
    currentTick = 0;
    PlayUpdateNextScoreTick(player);
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