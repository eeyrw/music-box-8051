.module SYNTH_CORE_ASM
.include "SynthCore.inc"
.include "Player.inc"
.globl _GenDecayEnvlopeAsm
.globl _NoteOnAsm
.globl _synthForAsm

.area IABS    (ABS,DATA)
.org SynthAbsAddr
_synthForAsm::
	.ds SynthTotalSize
.area CSEG    (CODE)
_GenDecayEnvlopeAsm:

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



envelopUpdateEnd$:	
						


pSynth = SynthAbsAddr
.irp  Idx,0,1,2,3,4,5,6,7
	pSndUnit = pSynth+Idx*unitSz
	mov a, (pSndUnit+pWavetablePos_int_l)
	clr c
	subb a,#WAVETABLE_ATTACK_LEN
	mov a, (pSndUnit+pWavetablePos_int_h)
	subb a,#(WAVETABLE_ATTACK_LEN>>8)
	jc 	loopGenDecayEnvlope_end'Idx'$
	mov a,(pSndUnit+pEnvelopePos)
	clr c
	subb a,#(ENVELOP_LEN-1)
	jnc loopGenDecayEnvlope_end'Idx'$
	mov dptr,#_EnvelopeTable
	mov a,(pSndUnit+pEnvelopePos)
	movc a,@a+dptr
	mov (pSndUnit+pEnvelopeLevel),a
	inc (pSndUnit+pEnvelopePos)

	loopGenDecayEnvlope_end'Idx'$:
.endm


ret

_NoteOnAsm:
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

	;dpl = note
	
	pSynth = SynthAbsAddr
	mov a,(pSynth+pLastSoundUnit)
	mov b,#unitSz
	mul ab
	add a,#pSynth
	mov r0,a ;base addr


	clr ea
	mov a,#0x7F
	anl a,dpl
	rl a
	mov r6,a
	mov dptr,#_WaveTable_Increment
	movc a,@a+dptr
	mov r4,a
	inc dptr
	mov a,r6
	movc a,@a+dptr
	mov r5,a

	mov a,#pIncrement_frac
	add a,r0
	mov r1,a
	mov a,r4
	mov @r1,a

	mov a,#pIncrement_int
	add a,r0
	mov r1,a
	mov a,r5
	mov @r1,a


	mov a,#pEnvelopeLevel
	add a,r0
	mov r1,a
	mov @r1,#255

	mov a,#pEnvelopePos
	add a,r0
	mov r1,a
	mov @r1,#0

	mov a,#pEnvelopePos
	add a,r0
	mov r1,a
	mov @r1,#0

	mov a,#pWavetablePos_int_l
	add a,r0
	mov r1,a
	mov @r1,#0

	mov a,#pWavetablePos_int_h
	add a,r0
	mov r1,a
	mov @r1,#0

	setb ea

	mov r4,(pSynth+pLastSoundUnit)
	inc r4
	mov a,r4
	clr c
	subb a,#POLY_NUM
	jnz lastSoundUnitUpdateEndNotEq$
	mov r4,#0
lastSoundUnitUpdateEndNotEq$:
	mov (pSynth+pLastSoundUnit),r4


ret
