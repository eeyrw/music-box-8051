#include "SynthCore.h"
#include <stdint.h>
#include <stdio.h>
#include "WaveTable.h"
#include "Bsp.h"
#include "CompressorGenerated.h"

MEM_XDATA(VoiceState) voiceState[POLY_NUM];

MEM_XDATA(uint16_t) AdsrAttackRateFrac;
MEM_XDATA(uint16_t) AdsrDecayRateFrac;
MEM_XDATA(uint16_t) AdsrReleaseRateFrac;
MEM_XDATA(uint16_t) AdsrSustainDecayRateFrac;
/* Tracks non-silent voices so ADSR ticks skip inactive XRAM state reads. */
static MEM_XDATA(uint8_t) activeVoiceMask;

/* 8x8 high-byte multiply helper; avoids SDCC's 16-bit __mulint call here. */
extern uint8_t MulU8High(uint8_t a, uint8_t b);

void AdsrInit(void)
{
	AdsrAttackRateFrac =
		(uint16_t)(((uint32_t)ADSR_ENV_MAX * ADSR_TICK_MS * ADSR_FRAC_DEN
			+ ADSR_ATTACK_MS - 1) / ADSR_ATTACK_MS);
	AdsrDecayRateFrac =
		(uint16_t)(((uint32_t)(ADSR_ENV_MAX - ADSR_SUSTAIN_THRESHOLD)
			* ADSR_TICK_MS * ADSR_FRAC_DEN + ADSR_DECAY_MS - 1) / ADSR_DECAY_MS);
	AdsrReleaseRateFrac =
		(uint16_t)(((uint32_t)ADSR_ENV_MAX * ADSR_TICK_MS * ADSR_FRAC_DEN
			+ ADSR_RELEASE_MS - 1) / ADSR_RELEASE_MS);
#if ADSR_SUSTAIN_DECAY_MS > 0
	AdsrSustainDecayRateFrac =
		(uint16_t)(((uint32_t)ADSR_SUSTAIN_THRESHOLD * ADSR_TICK_MS
			* ADSR_FRAC_DEN + ADSR_SUSTAIN_DECAY_MS - 1) / ADSR_SUSTAIN_DECAY_MS);
#else
	AdsrSustainDecayRateFrac = 0;
#endif
}

void AdsrSetRates(uint16_t attack, uint16_t decay, uint16_t sustainDecay, uint16_t release)
{
	AdsrAttackRateFrac = attack;
	AdsrDecayRateFrac = decay;
	AdsrSustainDecayRateFrac = sustainDecay;
	AdsrReleaseRateFrac = release;
}

#ifdef RUN_TEST
Synthesizer synthForC;
#endif

void SynthInit(Synthesizer *synth)
{
	SoundUnitUnion *soundUnionList = &(synth->SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		soundUnionList[i].combine.increment = 0;
		soundUnionList[i].combine.wavetablePos_frac = 0;
		soundUnionList[i].combine.wavetablePos_int = 0;
		soundUnionList[i].combine.envelopeLevel = 0;
		soundUnionList[i].combine.val = 0;
		voiceState[i].midiNote = 0;
		voiceState[i].velocity = 0;
		voiceState[i].envelopeState = ENV_STATE_SILENT;
		voiceState[i].envelopePhase = 0;
		voiceState[i].envelopeFrac = 0;
		voiceState[i].allocStamp = 0;
	}
	synth->lastSoundUnit = 0;
	synth->mixOut = 0;
	synth->lfsr = 0;
	synth->compressorEnv = 0;
	synth->compressorGain = SynthCompressorGainTable[0];
	synth->compressorTick = 0;
	synth->compressorPeak = 0;
	activeVoiceMask = 0;
}

void SynthDitherInit(Synthesizer *synth, uint16_t seed)
{
#if USE_DITHERING
	synth->lfsr = seed ? seed : 0xACE1;
#else
	(void)seed;
	synth->lfsr = 0;
#endif
}

static uint32_t nextTickMs;
static uint32_t nextCompressorTickMs;

static uint8_t compressorLevelFromMix(void)
{
	uint16_t mag;
	PlatformIrqState irq_state;

	Platform_IrqSave(irq_state);
	mag = synthForAsm.compressorPeak;
	synthForAsm.compressorPeak = 0;
	Platform_IrqRestore(irq_state);

	mag >>= SYNTH_COMPRESSOR_ENV_SHIFT;
	return mag > 255 ? 255 : (uint8_t)mag;
}

#define COMPRESSOR_ATTACK_STEP \
	((uint8_t)(((uint16_t)255 * COMPRESSOR_TICK_MS + COMPRESSOR_ATTACK_MS - 1) \
		/ COMPRESSOR_ATTACK_MS))
#define COMPRESSOR_RELEASE_STEP \
	((uint8_t)(((uint16_t)255 * COMPRESSOR_TICK_MS + COMPRESSOR_RELEASE_MS - 1) \
		/ COMPRESSOR_RELEASE_MS))

static void SynthCompressorTick(void)
{
	uint8_t level = compressorLevelFromMix();
	uint8_t env = synthForAsm.compressorEnv;
	uint8_t step;

	if (level > env) {
		step = level - env;
		if (step > COMPRESSOR_ATTACK_STEP)
			step = COMPRESSOR_ATTACK_STEP;
		env += step;
	} else if (env > level) {
		step = env - level;
		if (step > COMPRESSOR_RELEASE_STEP)
			step = COMPRESSOR_RELEASE_STEP;
		env -= step;
	}

	synthForAsm.compressorEnv = env;
	synthForAsm.compressorGain = SynthCompressorGainTable[env];
}

static void SynthControlTick(void)
{
	uint32_t now = GetSysMs();
	while ((int32_t)(now - nextCompressorTickMs) >= 0)
	{
		SynthCompressorTick();
		nextCompressorTickMs += COMPRESSOR_TICK_MS;
	}

	while ((int32_t)(now - nextTickMs) >= 0)
	{
		SynthEnvelopeStep();
		nextTickMs += ADSR_TICK_MS;
	}
}

static void SynthVisualize(void)
{
	uint16_t level = (uint16_t)synthForAsm.compressorEnv << 1;
	PWMA_CCR4H = (uint8_t)(level >> 8);
	PWMA_CCR4L = (uint8_t)level;
}

void SynthProcess(void)
{
	SynthControlTick();
	SynthVisualize();
}

void SynthEnvReset(void)
{
	PlatformIrqState irq_state;

	nextTickMs = GetSysMs();
	nextCompressorTickMs = nextTickMs;
	synthForAsm.compressorEnv = 0;
	synthForAsm.compressorGain = SynthCompressorGainTable[0];
	Platform_IrqSave(irq_state);
	synthForAsm.compressorPeak = 0;
	Platform_IrqRestore(irq_state);
}

void SynthReleaseAll(void)
{
	uint8_t i;
	for (i = 0; i < POLY_NUM; i++)
	{
		synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = 0;
		voiceState[i].envelopeState = ENV_STATE_SILENT;
	}
	activeVoiceMask = 0;
}

static uint8_t alloc_stamp;

void SynthPanic(void)
{
	uint16_t seed = GetRandom() | ((uint16_t)GetRandom() << 8);
	PlatformIrqState irq_state;

	Platform_IrqSave(irq_state);
	SynthReleaseAll();
	SynthInit(&synthForAsm);
	SynthEnvReset();
	SynthDitherInit(&synthForAsm, seed);
	alloc_stamp = 0;
	Platform_IrqRestore(irq_state);
}

#ifdef RUN_TEST
void SynthCoreTestReset(void)
{
	alloc_stamp = 0;
	AdsrInit();
	SynthInit(&synthForAsm);
	SynthDitherInit(&synthForAsm, 0xACE1);
}
#endif

#if NOTEON_STEAL_STRATEGY == VOICE_STEAL_OLDEST
static uint8_t stealVoice(void)
{
	uint8_t i, best;

	for (best = 0; best < POLY_NUM; best++)
	{
		if (voiceState[best].envelopeState != ENV_STATE_SILENT)
			break;
	}

	for (i = best + 1; i < POLY_NUM; i++)
	{
		if (voiceState[i].envelopeState == ENV_STATE_SILENT)
			continue;
		if ((uint8_t)(voiceState[i].allocStamp - voiceState[best].allocStamp) >= 128)
			best = i;
	}
	return best;
}
#endif

#if NOTEON_STEAL_STRATEGY == VOICE_STEAL_QUIETEST
static uint8_t stealVoice(void)
{
	uint8_t i, best;

	for (best = 0; best < POLY_NUM; best++)
	{
		if (voiceState[best].envelopeState != ENV_STATE_SILENT)
			break;
	}

	for (i = best + 1; i < POLY_NUM; i++)
	{
		if (voiceState[i].envelopeState == ENV_STATE_SILENT)
			continue;
		if (synthForAsm.SoundUnitUnionList[i].split.envelopeLevel <
		    synthForAsm.SoundUnitUnionList[best].split.envelopeLevel)
			best = i;
	}
	return best;
}
#endif

#if NOTEON_STEAL_STRATEGY == VOICE_STEAL_NEWEST
static uint8_t stealVoice(void)
{
	uint8_t i, best;

	for (best = 0; best < POLY_NUM; best++)
	{
		if (voiceState[best].envelopeState != ENV_STATE_SILENT)
			break;
	}

	for (i = best + 1; i < POLY_NUM; i++)
	{
		if (voiceState[i].envelopeState == ENV_STATE_SILENT)
			continue;
		if ((uint8_t)(voiceState[best].allocStamp - voiceState[i].allocStamp) >= 128)
			best = i;
	}
	return best;
}
#endif

#if NOTEON_STEAL_STRATEGY == VOICE_STEAL_HIGHEST_NOTE
static uint8_t stealVoice(void)
{
	uint8_t i, best;

	for (best = 0; best < POLY_NUM; best++)
	{
		if (voiceState[best].envelopeState != ENV_STATE_SILENT)
			break;
	}

	for (i = best + 1; i < POLY_NUM; i++)
	{
		if (voiceState[i].envelopeState == ENV_STATE_SILENT)
			continue;
		if (voiceState[i].midiNote > voiceState[best].midiNote)
			best = i;
	}
	return best;
}
#endif

#if NOTEON_STEAL_STRATEGY == VOICE_STEAL_LOWEST_NOTE
static uint8_t stealVoice(void)
{
	uint8_t i, best;

	for (best = 0; best < POLY_NUM; best++)
	{
		if (voiceState[best].envelopeState != ENV_STATE_SILENT)
			break;
	}

	for (i = best + 1; i < POLY_NUM; i++)
	{
		if (voiceState[i].envelopeState == ENV_STATE_SILENT)
			continue;
		if (voiceState[i].midiNote < voiceState[best].midiNote)
			best = i;
	}
	return best;
}
#endif

void SynthNoteOn(uint8_t note, uint8_t velocity)
{
	uint8_t idx;
	uint8_t vel_scaled;
	uint16_t inc;

	if (velocity == 0)
	{
		SynthNoteOff(note);
		return;
	}
	if (velocity > 127)
		velocity = 127;
	vel_scaled = (uint8_t)(velocity * 2);
	inc = WaveTable_Increment[note & 0x7F];

	idx = 0;
	while (idx < POLY_NUM && voiceState[idx].envelopeState != ENV_STATE_SILENT)
		idx++;

	if (idx >= POLY_NUM)
		idx = stealVoice();

	alloc_stamp++;
	if (alloc_stamp == 0)
		alloc_stamp = 1;

	synthForAsm.SoundUnitUnionList[idx].split.envelopeLevel = 0;
	synthForAsm.SoundUnitUnionList[idx].split.increment_frac = (uint8_t)(inc);
	synthForAsm.SoundUnitUnionList[idx].split.increment_int = (uint8_t)(inc >> 8);
	synthForAsm.SoundUnitUnionList[idx].split.wavetablePos_frac = 0;
	synthForAsm.SoundUnitUnionList[idx].split.wavetablePos_int = 0;

	voiceState[idx].midiNote = note;
	voiceState[idx].velocity = vel_scaled;
	voiceState[idx].envelopeState = ENV_STATE_ATTACK;
	voiceState[idx].envelopePhase = 0;
	voiceState[idx].envelopeFrac = 0;
	voiceState[idx].allocStamp = alloc_stamp;
	activeVoiceMask |= (uint8_t)(1U << idx);
}

void SynthNoteOff(uint8_t note)
{
	uint8_t i;
	for (i = 0; i < POLY_NUM; i++)
	{
		if (voiceState[i].envelopeState == ENV_STATE_SILENT)
			continue;
		if (voiceState[i].midiNote != note)
			continue;
		if (voiceState[i].envelopePhase == 0)
			voiceState[i].envelopePhase = (uint8_t)(ADSR_ENV_MAX >> 1);
		voiceState[i].envelopeFrac = 0;
		voiceState[i].envelopeState = ENV_STATE_RELEASE;
	}
}

void SynthEnvelopeStep(void)
{
	uint8_t i;
	MEM_XDATA(VoiceState) *voice = voiceState;
	MEM_FAST_DATA(SoundUnitUnion) *unit = synthForAsm.SoundUnitUnionList;
	uint8_t activeMask = activeVoiceMask;
	uint8_t voiceBit = 1;
	uint16_t sustainDecayRate;
	uint8_t sustainFlat;

	if (activeMask == 0)
		return;

	/* Cache sustain decay once per tick; runtime ADSR_SET can still change it next tick. */
	sustainDecayRate = AdsrSustainDecayRateFrac;
	sustainFlat = (sustainDecayRate == 0);

	for (i = 0; i < POLY_NUM; i++, voice++, unit++, voiceBit <<= 1)
	{
		uint8_t state;
		if (!(activeMask & voiceBit))
			continue;

		state = voice->envelopeState;

		if (state == ENV_STATE_SILENT)
		{
			activeMask &= (uint8_t)~voiceBit;
			continue;
		}

		if (state == ENV_STATE_SUSTAIN && sustainFlat)
			continue;

		uint8_t env = voice->envelopePhase;
		uint8_t vel = voice->velocity;
		uint8_t newState = state;
		uint8_t newEnv = env;
		uint8_t newFrac = voice->envelopeFrac;
		uint8_t carry;
		uint8_t envChanged;

		if (state == ENV_STATE_ATTACK)
		{
			uint16_t frac = (uint16_t)newFrac + AdsrAttackRateFrac;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			uint16_t v = (uint16_t)env + carry;
			if (v >= ADSR_ENV_MAX)
			{
				newEnv = ADSR_ENV_MAX;
				newState = ENV_STATE_DECAY;
				newFrac = 0;
			}
			else
			{
				newEnv = (uint8_t)v;
			}
		}
		else if (state == ENV_STATE_DECAY)
		{
			uint16_t frac = (uint16_t)newFrac + AdsrDecayRateFrac;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			if (carry >= env)
			{
				newEnv = ADSR_SUSTAIN_THRESHOLD;
				newState = ENV_STATE_SUSTAIN;
				newFrac = 0;
			}
			else
			{
				uint8_t v = env - carry;
				if (v <= ADSR_SUSTAIN_THRESHOLD)
				{
					newEnv = ADSR_SUSTAIN_THRESHOLD;
					newState = ENV_STATE_SUSTAIN;
					newFrac = 0;
				}
				else
				{
					newEnv = v;
				}
			}
		}
		else if (state == ENV_STATE_SUSTAIN)
		{
			uint16_t frac = (uint16_t)newFrac + sustainDecayRate;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			if (carry > 0)
			{
				if (carry >= env)
				{
					newEnv = 0;
					newState = ENV_STATE_SILENT;
				}
				else
				{
					newEnv = env - carry;
				}
			}
		}
		else if (state == ENV_STATE_RELEASE)
		{
			uint16_t frac = (uint16_t)newFrac + AdsrReleaseRateFrac;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			if (carry > 0)
			{
				if (carry >= env)
				{
					newEnv = 0;
					newState = ENV_STATE_SILENT;
				}
				else
				{
					newEnv = env - carry;
				}
			}
		}

		envChanged = (newEnv != env);

		voice->envelopeState = newState;
		voice->envelopePhase = newEnv;
		voice->envelopeFrac = newFrac;

		if (newState == ENV_STATE_SILENT || newEnv == 0)
		{
			unit->split.envelopeLevel = 0;
			if (newEnv == 0)
				voice->envelopeState = ENV_STATE_SILENT;
			activeMask &= (uint8_t)~voiceBit;
		}
		else if (envChanged)
		{
			/* Phase unchanged means curve index unchanged, so keep the old envelopeLevel. */
			uint8_t curve_idx = MulU8High(newEnv, vel);
			unit->split.envelopeLevel = NonlinearMapTable[curve_idx];
		}
	}
	activeVoiceMask = activeMask;
}
