.module SYNTH_CORE_ASM
.include "SynthCore.inc"
.globl _GenDecayEnvlopeAsm
.globl _NoteOnAsm

.area CODE
_GenDecayEnvlopeAsm:
	;clr a				; Register A as loop index.
	;ldw y,#_synthForAsm  		; Load sound unit pointer to register Y.#_synthForAsm is synthesizer object's address.
;loopGenDecayEnvlope$:
    ;cp a,#POLY_NUM
    ;jreq loopGenDecayEnvlope_end$
	;push a				; Keep a as temporary variable
	; loop body

	;    SoundUnitUnion* soundUnionList=&(synth->SoundUnitUnionList[0]);
	;	for (uint8_t i = 0; i < POLY_NUM; i++)
	;	{
	;		if(soundUnionList[i].combine.wavetablePos_int >= WAVETABLE_ATTACK_LEN &&
	;				soundUnionList[i].combine.envelopePos < sizeof(EnvelopeTable)-1)
	;		{
	;			soundUnionList[i].combine.envelopeLevel = EnvelopeTable[soundUnionList[i].combine.envelopePos];
	;			soundUnionList[i].combine.envelopePos += 1;
	;		}
	;	}

	;ldw x,y
	;ldw x,(pWavetablePos_int_h,x)
	;cpw x,_WAVETABLE_ATTACK_LEN
	;jrc envelopUpdateEnd$
	;ld a,(pEnvelopePos,y)
	;cp a,#(ENVELOP_LEN-1)
	;jrnc envelopUpdateEnd$
	;ld a,(pEnvelopePos,y)
	;clrw x
	;ld xl,a
	;ld a,(_EnvelopeTable,x)
	;ld (pEnvelopeLevel,y),a
	;inc (pEnvelopePos,y)

;envelopUpdateEnd$:	
						
	;pop a
    ;inc a
	;addw y,#SoundUnitSize
    ;jra loopGenDecayEnvlope$
;loopGenDecayEnvlope_end$:

ret

_NoteOnAsm:
	;ldw y,#_synthForAsm  		; Load sound unit pointer to register Y. #_synthForAsm is synthesizer object's address.
	;void NoteOn(Synthesizer* synth,uint8_t note)
	;{
	;	uint8_t lastSoundUnit = synth->lastSoundUnit;

	;	disable_interrupts();
	;	synth->SoundUnitUnionList[lastSoundUnit].combine.increment = PitchIncrementTable[note&0x7F];
	;	synth->SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_frac = 0;
	;	synth->SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_int = 0;
	;	synth->SoundUnitUnionList[lastSoundUnit].combine.envelopePos = 0;
	;	synth->SoundUnitUnionList[lastSoundUnit].combine.envelopeLevel = 255;
	;	enable_interrupts();

	;	lastSoundUnit++;

	;	if (lastSoundUnit== POLY_NUM)
	;		lastSoundUnit = 0;

	;	synth->lastSoundUnit=lastSoundUnit;
	;}
	;ldw x,#SoundUnitSize
	;ld a,(pLastSoundUnit,y)
	;mul x,a
	;addw x,#_synthForAsm 
	;ldw y,x
	;ld a,(0x03, sp) ;uint8_t note
	;sla a ;note*=2 and drop bit7
	;clrw x
	;ld xl,a
	;sim ;disable interrupt
	;ldw x,(_WaveTable_Increment,x)
	;ldw (pIncrement_int,y),x
	;clr (pWavetablePos_frac,y)
	;clr (pWavetablePos_int_h,y)
	;clr (pWavetablePos_int_l,y)
	;clr (pEnvelopePos,y)
	;ld a,#255
	;ld (pEnvelopeLevel,y),a
	;rim ;enable interrput

	;ldw y,#_synthForAsm 
	;ld a,(pLastSoundUnit,y)
	;inc a
	;cp a,#POLY_NUM
	;jrne lastSoundUnitUpdateEndNotEq$
	;clr a
;lastSoundUnitUpdateEndNotEq$:
	;ld (pLastSoundUnit,y),a


ret
