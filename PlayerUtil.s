.module PLAYER_UTILS
.globl _UpdateTick
.globl _PlayNoteTimingCheck
.globl _PlayUpdateNextScoreTick

; typedef struct _Player
; {
; 	uint8_t status;
;     uint32_t  currentTick;
; 	uint32_t  lastScoreTick;
; 	uint8_t  decayGenTick;
;     uint8_t* scorePointer;
;     Synthesizer mainSynthesizer;
; } Player;

pStatus=0
pCurrentTick_b3=1
pCurrentTick_b2=2
pCurrentTick_b1=3
pCurrentTick_b0=4
pLastScoreTick_b3=5
pLastScoreTick_b2=6
pLastScoreTick_b1=7
pLastScoreTick_b0=8
pDecayGenTick=9
pScorePointer=10
pMainSynthesizer=11
.area DATA

.area CODE

_UpdateTick:
; void Player32kProc(Player* player)
; {
;     Synth(&(player->mainSynthesizer));
;     player->currentTick++;
;     if(player->decayGenTick<200)
;         player->decayGenTick+=1;
; }
	ldw y,(0x03, sp) 		; Load sound unit pointer to register Y. (0x03, sp) is player object's address.
    ldw x,y
    ldw x,(pCurrentTick_b1,x)
    addw x,#1
    ldw (pCurrentTick_b1,y),x
    jrnc updateCurrentTickEnd$ 
    ldw x,y
    ldw x,(pCurrentTick_b3,x)
    addw x,#1
    ldw (pCurrentTick_b3,y),x    
updateCurrentTickEnd$:

ld a,(pDecayGenTick,y)
cp a,#150
jrnc updateDecayGenTickEnd$
inc (pDecayGenTick,y)

updateDecayGenTickEnd$:

ret

_PlayNoteTimingCheck:
;if((player->currentTick>>8)>=player->lastScoreTick)
	ldw y,(0x03, sp) 		; Load sound unit pointer to register Y. (0x03, sp) is player object's address.
    ld a,(pLastScoreTick_b2,y)
    cp a,(pCurrentTick_b3,y) ;Set flag C is (pCurrentTick_b3,y)>(pLastScoreTick_b2,y)
    jrc playNoteTimingCheckEndReturnTrue$
    ldw x,y
    ldw x,(pLastScoreTick_b1,x)
    cpw x,(pCurrentTick_b2,y)
    jrc playNoteTimingCheckEndReturnTrue$
    clr a
    ret
playNoteTimingCheckEndReturnTrue$:
    ld a,#0xff
    ret

_PlayUpdateNextScoreTick:
        ;    tempU32=player->lastScoreTick;
        ;     do
        ;    {
        ;        temp=*(player->scorePointer);
        ;        player->scorePointer++;
        ;        tempU32+=temp;
        ;    } while (temp==0xFF);
        ;    player->lastScoreTick=tempU32;

    push a  ;Keep a temp t=(0x01,sp)
    ldw y,(0x03+1, sp) 		; (0x03+1, sp) is player object's address.
varTickAccumulateLoop$:

    ldw x,y
    ldw x,(pScorePointer,x)
    ld a,(x)
    incw x
    ldw (pScorePointer,y),x
    ld (0x01,sp),a

    ld a,(pLastScoreTick_b0,y)
    add a,(0x01,sp)
    ld (pLastScoreTick_b0,y),a

    ld a,(pLastScoreTick_b1,y)
    adc a,#0
    ld (pLastScoreTick_b1,y),a

    ld a,(pLastScoreTick_b2,y)
    adc a,#0
    ld (pLastScoreTick_b2,y),a

    ld a,(pLastScoreTick_b3,y)
    adc a,#0
    ld (pLastScoreTick_b3,y),a
 
    ld a,(0x01,sp)
    cp a,#0xFF
    jreq varTickAccumulateLoop$

    pop a ;
    
    ret


