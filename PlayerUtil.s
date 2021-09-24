.module PLAYER_UTILS

.globl _PlayNoteTimingCheck
.globl _PlayUpdateNextScoreTick

.include "SynthCore.inc"
.include "Player.inc"


.area DSEG    (DATA)
_mainPlayer::
    .ds PlayerTotalSize
.area CSEG    (CODE)

_PlayNoteTimingCheck:
;if((currentTick>>8)>=player->lastScoreTick)
;	ldw y,(0x03, sp) 		; Load sound unit pointer to register Y. (0x03, sp) is player object's address.
;    ld a,(pLastScoreTick_b2,y)
;    cp a,_currentTick+0 ;Set flag C is (pCurrentTick_b3,y)>(pLastScoreTick_b2,y)
;    jrc playNoteTimingCheckEndReturnTrue$
;    ldw x,y
;    ldw x,(pLastScoreTick_b1,x)
;    cpw x,_currentTick+1
;    jrc playNoteTimingCheckEndReturnTrue$
;    clr a
;    ret
;playNoteTimingCheckEndReturnTrue$:
;    ld a,#0xff
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

;    push a  ;Keep a temp t=(0x01,sp)
;    ldw y,(0x03+1, sp) 		; (0x03+1, sp) is player object's address.
;varTickAccumulateLoop$:

;    ldw x,y
;    ldw x,(pScorePointer,x)
;    ld a,(x)
;    incw x
;    ldw (pScorePointer,y),x
;    ld (0x01,sp),a

;    ld a,(pLastScoreTick_b0,y)
;    add a,(0x01,sp)
;    ld (pLastScoreTick_b0,y),a

;    ld a,(pLastScoreTick_b1,y)
;    adc a,#0
;    ld (pLastScoreTick_b1,y),a

;    ld a,(pLastScoreTick_b2,y)
;    adc a,#0
;    ld (pLastScoreTick_b2,y),a

;    ld a,(pLastScoreTick_b3,y)
;    adc a,#0
;    ld (pLastScoreTick_b3,y),a
 
;    ld a,(0x01,sp)
;    cp a,#0xFF
;    jreq varTickAccumulateLoop$

;    pop a ;
    
    ret


