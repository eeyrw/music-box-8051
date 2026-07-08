#ifdef RUN_TEST

#include "SynthCore.h"
#include "PeriodTimer.h"
#include "WaveTable.h"
#include <stdint.h>
#include <stdio.h>

extern void UpdateTick(void);

#define USE_LINEAR_INTEROP_FOR_TEST 1

static uint16_t failures;
static __xdata Synthesizer expectedSynth;

#define ASSERT_TRUE(name, expr) do { \
	if (!(expr)) { \
		failures++; \
		printf("FAIL %s\n", name); \
	} \
} while (0)

#define ASSERT_EQ_U8(name, want, got) do { \
	uint8_t _want = (uint8_t)(want); \
	uint8_t _got = (uint8_t)(got); \
	if (_want != _got) { \
		failures++; \
		printf("FAIL %s: want=%u got=%u\n", name, _want, _got); \
	} \
} while (0)

#define ASSERT_EQ_U16(name, want, got) do { \
	uint16_t _want = (uint16_t)(want); \
	uint16_t _got = (uint16_t)(got); \
	if (_want != _got) { \
		failures++; \
		printf("FAIL %s: want=%u got=%u\n", name, _want, _got); \
	} \
} while (0)

#define ASSERT_EQ_I16(name, want, got) do { \
	int16_t _want = (int16_t)(want); \
	int16_t _got = (int16_t)(got); \
	if (_want != _got) { \
		failures++; \
		printf("FAIL %s: want=%d got=%d\n", name, _want, _got); \
	} \
} while (0)

static void reset_test_synth(void)
{
	SynthCoreTestReset();
	sysMsPre = 0;
	sysMs = 0;
}

static uint8_t curve_level(uint8_t env, uint8_t velocity_scaled)
{
	uint8_t idx = (uint8_t)(((uint16_t)env * velocity_scaled) >> 8);
	return AdsrCurveTable[idx];
}

static uint8_t high_s8_u8(int8_t a, uint8_t b)
{
	return (uint8_t)(((int16_t)a * (int16_t)b) >> 8);
}

static int8_t interpolate_sample(uint16_t pos, uint8_t frac)
{
	int8_t a = WaveTable[pos];
#if USE_LINEAR_INTEROP_FOR_TEST
	int8_t b = WaveTable[pos + 1];
	int8_t delta = (int8_t)(b - a);
	return (int8_t)(a + (int8_t)high_s8_u8(delta, frac));
#else
	(void)frac;
	return a;
#endif
}

static void synth_reference_step(Synthesizer *synth)
{
	int16_t mix = 0;
	uint8_t i;

	for (i = 0; i < POLY_NUM; i++) {
		SoundUnitUnion *unit = &synth->SoundUnitUnionList[i];
		uint8_t env = unit->split.envelopeLevel;

		if (env == 0)
			continue;

		int8_t sample = interpolate_sample(unit->split.wavetablePos_int,
			unit->split.wavetablePos_frac);
		mix += (int8_t)high_s8_u8(sample, env);

		uint16_t pos = unit->split.wavetablePos_int;
		uint16_t frac = (uint16_t)unit->split.wavetablePos_frac
			+ unit->split.increment_frac;

		pos += unit->split.increment_int + (uint8_t)(frac >> 8);
		if (pos >= WAVETABLE_LEN)
			pos -= WAVETABLE_LOOP_LEN;

		unit->split.wavetablePos_frac = (uint8_t)frac;
		unit->split.wavetablePos_int = pos;
	}

	synth->mixOut = mix;
}

static void copy_synth(Synthesizer *dst, Synthesizer *src)
{
	uint8_t i;

	for (i = 0; i < POLY_NUM; i++)
		dst->SoundUnitUnionList[i] = src->SoundUnitUnionList[i];
	dst->mixOut = src->mixOut;
	dst->lastSoundUnit = src->lastSoundUnit;
	dst->lfsr = src->lfsr;
}

static void compare_synth_step(const char *name, Synthesizer *expected)
{
	uint8_t i;

	ASSERT_EQ_I16(name, expected->mixOut, synthForAsm.mixOut);
	for (i = 0; i < POLY_NUM; i++) {
		SoundUnitUnion *want = &expected->SoundUnitUnionList[i];
		SoundUnitUnion *got = &synthForAsm.SoundUnitUnionList[i];

		ASSERT_EQ_U8("synth frac", want->split.wavetablePos_frac,
			got->split.wavetablePos_frac);
		ASSERT_EQ_U16("synth pos", want->split.wavetablePos_int,
			got->split.wavetablePos_int);
	}
}

void TestUpdateTickFunc(void)
{
	uint8_t i;

	printf("TestUpdateTickFunc\n");
	sysMsPre = 0;
	sysMs = 0;

	for (i = 0; i < 31; i++)
		UpdateTick();
	ASSERT_EQ_U8("pre before ms", 31, sysMsPre);
	ASSERT_EQ_U16("ms before divide", 0, (uint16_t)sysMs);

	UpdateTick();
	ASSERT_EQ_U8("pre reset", 0, sysMsPre);
	ASSERT_EQ_U16("ms increment", 1, (uint16_t)sysMs);

	sysMsPre = 31;
	sysMs = 0xffffffffUL;
	UpdateTick();
	ASSERT_EQ_U8("pre wrap reset", 0, sysMsPre);
	ASSERT_TRUE("ms wrap", sysMs == 0);
}

static void TestNoteOnAllocation(void)
{
	uint8_t i;

	printf("TestNoteOnAllocation\n");
	reset_test_synth();

	NoteOnAsm(60, 127);
	ASSERT_EQ_U8("voice0 note", 60, voiceState[0].midiNote);
	ASSERT_EQ_U8("voice0 vel", 254, voiceState[0].velocity);
	ASSERT_EQ_U8("voice0 attack", ENV_STATE_ATTACK, voiceState[0].envelopeState);
	ASSERT_EQ_U8("voice0 phase", 0, voiceState[0].envelopePhase);
	ASSERT_EQ_U8("voice0 level", 0, synthForAsm.SoundUnitUnionList[0].split.envelopeLevel);
	ASSERT_EQ_U16("voice0 inc", WaveTable_Increment[60],
		synthForAsm.SoundUnitUnionList[0].combine.increment);

	reset_test_synth();
	for (i = 0; i < POLY_NUM; i++)
		NoteOnAsm((uint8_t)(60 + i), 100);
	for (i = 0; i < POLY_NUM; i++) {
		ASSERT_EQ_U8("fill note", 60 + i, voiceState[i].midiNote);
		ASSERT_EQ_U8("fill state", ENV_STATE_ATTACK, voiceState[i].envelopeState);
	}

	NoteOnAsm(90, 127);
	ASSERT_EQ_U8("oldest stolen note", 90, voiceState[0].midiNote);
	ASSERT_EQ_U8("oldest stolen state", ENV_STATE_ATTACK, voiceState[0].envelopeState);
}

static void TestNoteOffRelease(void)
{
	printf("TestNoteOffRelease\n");
	reset_test_synth();

	NoteOnAsm(64, 90);
	NoteOnAsm(64, 120);
	NoteOffAsm(64);
	ASSERT_EQ_U8("release first", ENV_STATE_RELEASE, voiceState[0].envelopeState);
	ASSERT_EQ_U8("release second", ENV_STATE_RELEASE, voiceState[1].envelopeState);
	ASSERT_EQ_U8("short note precharge", ADSR_ENV_MAX >> 1, voiceState[0].envelopePhase);
	ASSERT_EQ_U8("short note precharge 2", ADSR_ENV_MAX >> 1, voiceState[1].envelopePhase);

	reset_test_synth();
	NoteOnAsm(67, 80);
	NoteOnAsm(67, 0);
	ASSERT_EQ_U8("vel0 noteoff", ENV_STATE_RELEASE, voiceState[0].envelopeState);
}

static void TestAdsrStateMachine(void)
{
	printf("TestAdsrStateMachine\n");
	reset_test_synth();
	AdsrSetRates(64U << 8, 14U << 8, 0, 32U << 8);

	NoteOnAsm(72, 127);
	GenDecayEnvlopeAsm();
	ASSERT_EQ_U8("attack phase 1", 64, voiceState[0].envelopePhase);
	ASSERT_EQ_U8("attack state 1", ENV_STATE_ATTACK, voiceState[0].envelopeState);
	ASSERT_EQ_U8("attack level 1", curve_level(64, 254),
		synthForAsm.SoundUnitUnionList[0].split.envelopeLevel);

	GenDecayEnvlopeAsm();
	ASSERT_EQ_U8("attack reaches decay", ADSR_ENV_MAX, voiceState[0].envelopePhase);
	ASSERT_EQ_U8("decay state", ENV_STATE_DECAY, voiceState[0].envelopeState);

	GenDecayEnvlopeAsm();
	ASSERT_EQ_U8("decay phase", 114, voiceState[0].envelopePhase);
	ASSERT_EQ_U8("decay level", curve_level(114, 254),
		synthForAsm.SoundUnitUnionList[0].split.envelopeLevel);

	GenDecayEnvlopeAsm();
	ASSERT_EQ_U8("sustain threshold", ADSR_SUSTAIN_THRESHOLD, voiceState[0].envelopePhase);
	ASSERT_EQ_U8("sustain state", ENV_STATE_SUSTAIN, voiceState[0].envelopeState);

	NoteOffAsm(72);
	ASSERT_EQ_U8("release state", ENV_STATE_RELEASE, voiceState[0].envelopeState);
	GenDecayEnvlopeAsm();
	ASSERT_EQ_U8("release phase", 68, voiceState[0].envelopePhase);
	GenDecayEnvlopeAsm();
	GenDecayEnvlopeAsm();
	GenDecayEnvlopeAsm();
	ASSERT_EQ_U8("release silent", ENV_STATE_SILENT, voiceState[0].envelopeState);
	ASSERT_EQ_U8("release level zero", 0,
		synthForAsm.SoundUnitUnionList[0].split.envelopeLevel);
}

static void setup_synth_case(uint8_t idx, uint16_t pos, uint8_t frac,
	uint16_t inc, uint8_t env)
{
	SoundUnitUnion *unit = &synthForAsm.SoundUnitUnionList[idx];

	unit->combine.increment = inc;
	unit->split.wavetablePos_frac = frac;
	unit->split.wavetablePos_int = pos;
	unit->split.envelopeLevel = env;
}

static void TestSynthAsmStep(void)
{
	printf("TestSynthAsmStep\n");
	reset_test_synth();
	setup_synth_case(0, 0, 0, 0x0100, 128);
	copy_synth(&expectedSynth, &synthForAsm);
	synth_reference_step(&expectedSynth);
	SynthAsm();
	compare_synth_step("single voice", &expectedSynth);

	reset_test_synth();
	setup_synth_case(0, 10, 128, 0x0180, 200);
	setup_synth_case(1, 400, 64, 0x0040, 127);
	copy_synth(&expectedSynth, &synthForAsm);
	synth_reference_step(&expectedSynth);
	SynthAsm();
	compare_synth_step("multi voice", &expectedSynth);

	reset_test_synth();
	setup_synth_case(0, WAVETABLE_LEN - 1, 250, 0x020a, 255);
	copy_synth(&expectedSynth, &synthForAsm);
	synth_reference_step(&expectedSynth);
	SynthAsm();
	compare_synth_step("wrap voice", &expectedSynth);
}

void TestProcess(void)
{
	failures = 0;
	printf("Synth core tests start\n");

	TestUpdateTickFunc();
	TestNoteOnAllocation();
	TestNoteOffRelease();
	TestAdsrStateMachine();
	TestSynthAsmStep();

	if (failures == 0)
		printf("ALL TESTS PASSED\n");
	else
		printf("TESTS FAILED: %u\n", failures);
}

#endif
