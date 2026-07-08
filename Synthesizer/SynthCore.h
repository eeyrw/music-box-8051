#ifndef __SYNTH_CORE_H__
#define __SYNTH_CORE_H__

#include <stdint.h>

#define POLY_NUM 8

// ================================================================
// ADSR 包络参数 — 修改下面三个时长即可
//   实际时长 ≈ ceil(幅度/步进) × ADSR_TICK_MS
//   ADSR_TICK_MS 在 Player.c 中控制 GenDecayEnvlopeAsm 调用频率
//   ADSR_*_MS 无上限 (仅用于编译期推导步进, 不占 RAM 也不走串口)
// ================================================================

#define ADSR_TICK_MS              5
#define ADSR_ATTACK_MS           20
#define ADSR_DECAY_MS            60
#define ADSR_RELEASE_MS         200
#define ADSR_SUSTAIN_DECAY_MS    5000

#define ADSR_ENV_MAX            128
#define ADSR_SUSTAIN_THRESHOLD  100

#define ADSR_FRAC_DEN  256

extern __xdata uint16_t AdsrAttackRateFrac;
extern __xdata uint16_t AdsrDecayRateFrac;
extern __xdata uint16_t AdsrReleaseRateFrac;
extern __xdata uint16_t AdsrSustainDecayRateFrac;

void AdsrInit(void);

// ---- ADSR 包络状态 ----
#define ENV_STATE_SILENT  0
#define ENV_STATE_ATTACK  1
#define ENV_STATE_DECAY   2
#define ENV_STATE_SUSTAIN 3
#define ENV_STATE_RELEASE 4

// ---- Voice Stealing Strategy (Phase B fallback when all voices busy) ----
#define VOICE_STEAL_OLDEST       0   // FIFO timestamp: steal oldest-allocated voice
#define VOICE_STEAL_QUIETEST     1   // Envelope level: steal quietest voice
#define VOICE_STEAL_NEWEST       2   // FIFO timestamp: steal most-recently-allocated voice
#define VOICE_STEAL_HIGHEST_NOTE 3   // MIDI note: steal highest pitch (less perceptually missed)
#define VOICE_STEAL_LOWEST_NOTE  4   // MIDI note: steal lowest pitch

#define NOTEON_STEAL_STRATEGY VOICE_STEAL_QUIETEST

// ---- 力度曲线选择 ----
#define VEL_CURVE_POWER2  0
#define VEL_CURVE_POWER06 1
#define VEL_CURVE_DB20    2

#define VELOCITY_CURVE VEL_CURVE_DB20

extern const __code uint8_t AdsrCurveTable[128];

typedef struct _SoundUnit
{
	uint16_t increment;
	uint8_t wavetablePos_frac;
	uint16_t wavetablePos_int;
	uint8_t envelopeLevel;
	int16_t val;
	int8_t sampleVal;
} SoundUnit;

typedef struct _SoundUnitSplit
{
	uint8_t increment_frac;
	uint8_t increment_int;
	uint8_t wavetablePos_frac;
	uint16_t wavetablePos_int;
	uint8_t envelopeLevel;
	int16_t val;
	int8_t sampleVal;
} SoundUnitSplit;

typedef union _SoundUnitUnion
{
	SoundUnit combine;
	SoundUnitSplit split;
} SoundUnitUnion;

typedef struct _Synthesizer
{
	SoundUnitUnion SoundUnitUnionList[POLY_NUM];
	int16_t mixOut;
	uint8_t lastSoundUnit;
	uint16_t lfsr;
} Synthesizer;

typedef struct _VoiceState
{
	uint8_t midiNote;
	uint8_t velocity;
	uint8_t envelopeState;
	uint8_t envelopePhase;
	uint8_t envelopeFrac;
	uint8_t allocStamp;
} VoiceState;

extern __xdata VoiceState voiceState[POLY_NUM];

extern void SynthInit(Synthesizer *synth);
extern void SynthDitherInit(Synthesizer *synth, uint16_t seed);
extern void SynthEnvelopeTick(void);
extern void SynthEnvReset(void);

#ifdef RUN_TEST
extern void NoteOnC(uint8_t note);
extern void NoteOffC(uint8_t note);
extern void SynthC(void);
extern void GenDecayEnvlopeC(void);
#endif

extern void NoteOnAsm(uint8_t note, uint8_t velocity);
extern void NoteOffAsm(uint8_t note);
extern void GenDecayEnvlopeAsm(void);
extern void SynthReleaseAllAsm(void);
extern void SynthAsm(void);

#ifdef RUN_TEST
extern Synthesizer synthForC;
#endif

extern __data Synthesizer synthForAsm;

#endif
