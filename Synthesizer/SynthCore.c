#include "SynthCore.h"
#include <stdint.h>
#include <stdio.h>
#include "WaveTable.h"
#include "Bsp.h"

__xdata VoiceState voiceState[POLY_NUM];

__xdata uint16_t AdsrAttackRateFrac;
__xdata uint16_t AdsrDecayRateFrac;
__xdata uint16_t AdsrReleaseRateFrac;
__xdata uint16_t AdsrSustainDecayRateFrac;

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

#if VELOCITY_CURVE == VEL_CURVE_POWER2
const __code uint8_t AdsrCurveTable[128] = {  /* level = (x/127)^2 * 255 */
      0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
      2,   2,   3,   3,   4,   4,   5,   5,   6,   6,   7,   8,
      9,   9,  10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
     20,  21,  22,  24,  25,  26,  27,  29,  30,  32,  33,  34,
     36,  37,  39,  41,  42,  44,  46,  47,  49,  51,  53,  55,
     56,  58,  60,  62,  64,  66,  68,  70,  73,  75,  77,  79,
     81,  84,  86,  88,  91,  93,  96,  98, 101, 103, 106, 108,
    111, 114, 116, 119, 122, 125, 128, 130, 133, 136, 139, 142,
    145, 148, 151, 154, 158, 161, 164, 167, 171, 174, 177, 181,
    184, 187, 191, 194, 198, 201, 205, 209, 212, 216, 220, 223,
    227, 231, 235, 239, 243, 247, 251, 255
};
#elif VELOCITY_CURVE == VEL_CURVE_POWER06
const __code uint8_t AdsrCurveTable[128] = {  /* level = (x/127)^0.6 * 255 */
      0,  13,  21,  26,  32,  36,  40,  44,  48,  52,  55,  58,
     61,  64,  67,  70,  73,  76,  78,  81,  84,  86,  89,  91,
     93,  96,  98, 100, 102, 105, 107, 109, 111, 113, 115, 117,
    119, 121, 123, 125, 127, 129, 131, 133, 134, 136, 138, 140,
    142, 144, 145, 147, 149, 150, 152, 154, 156, 157, 159, 160,
    162, 164, 165, 167, 169, 170, 172, 173, 175, 176, 178, 179,
    181, 182, 184, 185, 187, 188, 190, 191, 193, 194, 196, 197,
    198, 200, 201, 203, 204, 206, 207, 208, 210, 211, 212, 214,
    215, 216, 218, 219, 220, 222, 223, 224, 226, 227, 228, 230,
    231, 232, 233, 235, 236, 237, 239, 240, 241, 242, 243, 245,
    246, 247, 248, 250, 251, 252, 253, 255
};
#else
const __code uint8_t AdsrCurveTable[128] = {  /* level = 10^((x/127-1)*1.0) * 255  (-20dB) */
      0,  26,  26,  26,  27,  27,  28,  28,  29,  29,  30,  31,
     31,  32,  32,  33,  33,  34,  35,  35,  36,  37,  37,  38,
     39,  39,  40,  41,  42,  42,  43,  44,  45,  46,  46,  47,
     48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  65,  67,  68,  69,  70,  72,  73,
     74,  76,  77,  78,  80,  81,  83,  84,  86,  87,  89,  91,
     92,  94,  96,  98,  99, 101, 103, 105, 107, 109, 111, 113,
    115, 117, 119, 121, 123, 126, 128, 130, 133, 135, 138, 140,
    143, 145, 148, 151, 153, 156, 159, 162, 165, 168, 171, 174,
    177, 181, 184, 187, 191, 194, 198, 201, 205, 209, 213, 217,
    221, 225, 229, 233, 237, 242, 246, 255
};
#endif

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
}

void SynthDitherInit(Synthesizer *synth, uint16_t seed)
{
	synth->lfsr = seed ? seed : 0xACE1;
}

static uint32_t nextTickMs;

void SynthEnvelopeTick(void)
{
	uint32_t now = GetSysMs();
	while ((int32_t)(now - nextTickMs) >= 0)
	{
		GenDecayEnvlopeAsm();
		nextTickMs += ADSR_TICK_MS;
	}
}

void SynthEnvReset(void)
{
	nextTickMs = GetSysMs();
}

void SynthReleaseAllAsm(void)
{
	uint8_t i;
	for (i = 0; i < POLY_NUM; i++)
	{
		synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = 0;
		voiceState[i].envelopeState = ENV_STATE_SILENT;
	}
}

static uint8_t alloc_stamp;

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

void NoteOnAsm(uint8_t note, uint8_t velocity)
{
	uint8_t idx;
	uint8_t vel_scaled;
	uint16_t inc;

	if (velocity == 0)
	{
		NoteOffAsm(note);
		return;
	}
	if (velocity > 127)
		velocity = 127;
	vel_scaled = (uint8_t)(velocity * 2);

	idx = 0;
	while (idx < POLY_NUM && voiceState[idx].envelopeState != ENV_STATE_SILENT)
		idx++;

	if (idx >= POLY_NUM)
		idx = stealVoice();

	alloc_stamp++;
	if (alloc_stamp == 0)
		alloc_stamp = 1;

	EA = 0;

	inc = WaveTable_Increment[note & 0x7F];
	synthForAsm.SoundUnitUnionList[idx].split.increment_frac = (uint8_t)(inc);
	synthForAsm.SoundUnitUnionList[idx].split.increment_int = (uint8_t)(inc >> 8);
	synthForAsm.SoundUnitUnionList[idx].split.wavetablePos_frac = 0;
	synthForAsm.SoundUnitUnionList[idx].split.wavetablePos_int = 0;
	synthForAsm.SoundUnitUnionList[idx].split.envelopeLevel = 0;

	voiceState[idx].midiNote = note;
	voiceState[idx].velocity = vel_scaled;
	voiceState[idx].envelopeState = ENV_STATE_ATTACK;
	voiceState[idx].envelopePhase = 0;
	voiceState[idx].envelopeFrac = 0;
	voiceState[idx].allocStamp = alloc_stamp;

	EA = 1;
}

void NoteOffAsm(uint8_t note)
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

void GenDecayEnvlopeAsm(void)
{
	uint8_t i;
	for (i = 0; i < POLY_NUM; i++)
	{
		uint8_t state = voiceState[i].envelopeState;
		if (state == ENV_STATE_SILENT)
			continue;

		uint8_t env = voiceState[i].envelopePhase;
		uint8_t vel = voiceState[i].velocity;
		uint8_t newState = state;
		uint8_t newEnv = env;
		uint8_t newFrac = voiceState[i].envelopeFrac;
		uint8_t carry;

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
			if (AdsrSustainDecayRateFrac > 0)
			{
			uint16_t frac = (uint16_t)newFrac + AdsrSustainDecayRateFrac;
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

		voiceState[i].envelopeState = newState;
		voiceState[i].envelopePhase = newEnv;
		voiceState[i].envelopeFrac = newFrac;

		if (newState == ENV_STATE_SILENT || newEnv == 0)
		{
			synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = 0;
			if (newEnv == 0)
				voiceState[i].envelopeState = ENV_STATE_SILENT;
		}
		else
		{
			uint8_t curve_idx = (uint8_t)(((uint16_t)newEnv * vel) >> 8);
			synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = AdsrCurveTable[curve_idx];
		}
	}
}

#ifdef RUN_TEST
static uint8_t selectVoice(Synthesizer *synth)
{
	uint8_t i;
	for (i = 0; i < POLY_NUM; i++) {
		if (synth->SoundUnitUnionList[i].split.envelopeLevel == 0)
			return i;
	}
	return synth->lastSoundUnit;
}

void NoteOnAsmP(uint8_t note)
{
	uint8_t idx = selectVoice(&synthForAsm);

	synthForAsm.SoundUnitUnionList[idx].combine.increment = WaveTable_Increment[note & 0x7F];
	synthForAsm.SoundUnitUnionList[idx].combine.wavetablePos_frac = 0;
	synthForAsm.SoundUnitUnionList[idx].combine.wavetablePos_int = 0;
	synthForAsm.SoundUnitUnionList[idx].combine.envelopeLevel = 0;

	voiceState[idx].midiNote = note;
	voiceState[idx].velocity = (uint8_t)(127 * 2);
	voiceState[idx].envelopeState = ENV_STATE_ATTACK;
	voiceState[idx].envelopePhase = 0;
	voiceState[idx].envelopeFrac = 0;

	idx++;
	if (idx == POLY_NUM)
		idx = 0;
	synthForAsm.lastSoundUnit = idx;
}

void GenDecayEnvlopeAsmP(void)
{
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		uint8_t state = voiceState[i].envelopeState;
		if (state == ENV_STATE_SILENT)
			continue;

		uint8_t env = voiceState[i].envelopePhase;
		uint8_t vel  = voiceState[i].velocity;
		uint8_t newState = state;
		uint8_t newEnv = env;
		uint8_t newFrac = voiceState[i].envelopeFrac;
		uint8_t carry;

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
			}
			else
			{
				uint8_t v = env - carry;
				if (v <= ADSR_SUSTAIN_THRESHOLD)
				{
					newEnv = ADSR_SUSTAIN_THRESHOLD;
					newState = ENV_STATE_SUSTAIN;
				}
				else
				{
					newEnv = v;
				}
			}
		}
		else if (state == ENV_STATE_SUSTAIN)
		{
			if (AdsrSustainDecayRateFrac > 0)
			{
			uint16_t frac = (uint16_t)newFrac + AdsrSustainDecayRateFrac;
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

		voiceState[i].envelopeState = newState;
		voiceState[i].envelopePhase = newEnv;
		voiceState[i].envelopeFrac = newFrac;

		if (newState == ENV_STATE_SILENT)
		{
			synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = 0;
		}
		else
		{
			if (newEnv == 0)
			{
				synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = 0;
				voiceState[i].envelopeState = ENV_STATE_SILENT;
			}
			else
			{
				uint8_t idx = (uint8_t)(((uint16_t)newEnv * vel) >> 8);
				synthForAsm.SoundUnitUnionList[i].split.envelopeLevel = AdsrCurveTable[idx];
			}
		}
	}
}

void NoteOffAsmP(uint8_t note)
{
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (synthForAsm.SoundUnitUnionList[i].split.envelopeLevel > 0 &&
			voiceState[i].midiNote == note)
		{
			if (voiceState[i].envelopePhase == 0)
				voiceState[i].envelopePhase = ADSR_ENV_MAX >> 1;
			voiceState[i].envelopeState = ENV_STATE_RELEASE;
		}
	}
}

void NoteOnC(uint8_t note)
{
	uint8_t idx = selectVoice(&synthForC);

	synthForC.SoundUnitUnionList[idx].combine.increment = WaveTable_Increment[note & 0x7F];
	synthForC.SoundUnitUnionList[idx].combine.wavetablePos_frac = 0;
	synthForC.SoundUnitUnionList[idx].combine.wavetablePos_int = 0;
	synthForC.SoundUnitUnionList[idx].combine.envelopeLevel = 0;

	idx++;
	if (idx == POLY_NUM)
		idx = 0;
	synthForC.lastSoundUnit = idx;
}

void NoteOffC(uint8_t note)
{
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (synthForC.SoundUnitUnionList[i].combine.envelopeLevel > 0)
		{
			synthForC.SoundUnitUnionList[i].combine.envelopeLevel = 0;
		}
	}
}

void SynthC(void)
{
	synthForC.mixOut = 0;
	SoundUnitUnion *soundUnionList = &(synthForC.SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (soundUnionList[i].combine.envelopeLevel == 0)
			continue;
		soundUnionList[i].combine.sampleVal = WaveTable[soundUnionList[i].combine.wavetablePos_int];
		soundUnionList[i].combine.val = (int16_t)soundUnionList[i].combine.envelopeLevel * soundUnionList[i].combine.sampleVal;

		uint32_t waveTablePos = soundUnionList[i].combine.increment +
								soundUnionList[i].combine.wavetablePos_frac +
								((uint32_t)soundUnionList[i].combine.wavetablePos_int << 8);

		uint16_t waveTablePosInt = waveTablePos >> 8;
		if (waveTablePosInt >= WAVETABLE_LEN)
			waveTablePosInt -= WAVETABLE_LOOP_LEN;
		soundUnionList[i].combine.wavetablePos_int = waveTablePosInt;
		soundUnionList[i].combine.wavetablePos_frac = 0xFF & waveTablePos;
		synthForC.mixOut += (soundUnionList[i].combine.val >> 8);
	}
}

void GenDecayEnvlopeC(void)
{
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		uint8_t state = voiceState[i].envelopeState;
		if (state == ENV_STATE_SILENT)
			continue;

		uint8_t env = voiceState[i].envelopePhase;
		uint8_t vel  = voiceState[i].velocity;
		uint8_t newState = state;
		uint8_t newEnv = env;
		uint8_t newFrac = voiceState[i].envelopeFrac;
		uint8_t carry;

		if (state == ENV_STATE_ATTACK)
		{
			uint16_t frac = (uint16_t)newFrac + AdsrAttackRateFrac;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			uint16_t v = (uint16_t)env + carry;
			if (v >= ADSR_ENV_MAX) { newEnv = ADSR_ENV_MAX; newState = ENV_STATE_DECAY; }
			else { newEnv = (uint8_t)v; }
		}
		else if (state == ENV_STATE_DECAY)
		{
			uint16_t frac = (uint16_t)newFrac + AdsrDecayRateFrac;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			if (carry >= env) { newEnv = ADSR_SUSTAIN_THRESHOLD; newState = ENV_STATE_SUSTAIN; }
			else
			{
				uint8_t v = env - carry;
				if (v <= ADSR_SUSTAIN_THRESHOLD) { newEnv = ADSR_SUSTAIN_THRESHOLD; newState = ENV_STATE_SUSTAIN; }
				else { newEnv = v; }
			}
		}
		else if (state == ENV_STATE_SUSTAIN)
		{
			if (AdsrSustainDecayRateFrac > 0)
			{
			uint16_t frac = (uint16_t)newFrac + AdsrSustainDecayRateFrac;
			carry = (uint8_t)(frac >> 8);
			newFrac = (uint8_t)frac;
			if (carry > 0)
			{
				if (carry >= env) { newEnv = 0; newState = ENV_STATE_SILENT; }
				else { newEnv = env - carry; }
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
				if (carry >= env) { newEnv = 0; newState = ENV_STATE_SILENT; }
				else { newEnv = env - carry; }
			}
		}

		voiceState[i].envelopeState = newState;
		voiceState[i].envelopePhase = newEnv;
		voiceState[i].envelopeFrac = newFrac;

		if (newState == ENV_STATE_SILENT || newEnv == 0)
		{
			synthForC.SoundUnitUnionList[i].split.envelopeLevel = 0;
			if (newEnv == 0) voiceState[i].envelopeState = ENV_STATE_SILENT;
		}
		else
		{
			uint8_t idx = (uint8_t)(((uint16_t)newEnv * vel) >> 8);
			synthForC.SoundUnitUnionList[i].split.envelopeLevel = AdsrCurveTable[idx];
		}
	}
}
#endif
