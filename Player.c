#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "SynthCore.h"
#include "Player.h"

extern unsigned char Score[];

void Player32kProc(Player *player)
{
    SynthAsm(&(player->mainSynthesizer));
    UpdateTick(player);
}

void PlayerProcess(Player *player)
{

    uint8_t temp;
    
    if (player->decayGenTick >= 150)
    {
        GenDecayEnvlopeAsm(&(player->mainSynthesizer));
        player->decayGenTick = 0;
    }
    if (player->status == STATUS_PLAYING)
    {
        if(PlayNoteTimingCheck(player))
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
                    NoteOnAsm(&(player->mainSynthesizer), temp);
                }
            } while ((temp & 0x80) == 0);
            PlayUpdateNextScoreTick(player);
        }
    }
}

void PlayerPlay(Player *player)
{
    player->currentTick = 0;
    player->lastScoreTick = 0;
    player->decayGenTick = 0;
    player->scorePointer = Score;
    PlayUpdateNextScoreTick(player);
    player->status = STATUS_PLAYING;
}

void PlayerInit(Player *player)
{
    player->status = STATUS_STOP;
    player->currentTick = 0;
    player->lastScoreTick = 0;
    player->decayGenTick = 0;
    player->scorePointer = Score;
    SynthInit(&(player->mainSynthesizer));
}