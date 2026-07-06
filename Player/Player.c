//====================================================================
// Player.c — SSCR 乐谱解码器 + SSPL 多曲播放调度器
//
// 数据流:
//   Score[] (SSPL 容器)
//     → SSPL header 解析 (入口表)
//       → PlayScore(sscrScore) 初始化 SSCR 解码器
//         → SSCR_DecodeProcess() 逐 tick 解码事件流
//         → NoteOnAsm(note) 触发音符
//         → NoteOffAsm(note) 关闭音符
//
// 调度时序:
//   GetSysMs() 获取系统毫秒时间 (Timer0 ISR 驱动)
//   SSCR delta (raw tick) × 8 → 毫秒 (TickPerSecond=125, 1000/125=8ms/tick)
//
// SSCR 事件编码:
//   0x80-0xBD  NoteOff 直连 (note = byte & 0x3F)
//   0xBE       EndOfScore
//   0xBF       NoteOff 扩展 (下一字节 = note)
//   0xC0-0xFD  NoteOn  直连 (note = byte & 0x3F)
//   0xFE       保留
//   0xFF       NoteOn  扩展 (下一字节 = note)
//====================================================================

#include <stdint.h>
#include "SynthCore.h"
#include "Player.h"
#include "RegisterDefine.h"
#include "Bsp.h"

static uint32_t lastDecayMs;

#if VELOCITY_CURVE == VEL_CURVE_POWER2
const __code uint8_t velocityCurve[128] = {  /* level = (vel/127)^2 * 255 */
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
const __code uint8_t velocityCurve[128] = {  /* level = (vel/127)^0.6 * 255 */
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
const __code uint8_t velocityCurve[128] = {  /* level = 10^((vel/127-1)*1.0) * 255  (-20dB) */
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

/* ================================================================
 * SSCR 解码器内部
 * ================================================================ */

static uint8_t sscr_read_byte(SSCR_Player *d, uint8_t *out)
{
    return stream_read(&d->stream, out);
}

static uint8_t sscr_peek_byte(SSCR_Player *d, uint8_t *out)
{
    return stream_peek(&d->stream, out);
}

static uint32_t sscr_read_delta(SSCR_Player *d)
{
    uint32_t value = 0;
    uint8_t shift = 0;
    uint8_t byte;

    while (sscr_read_byte(d, &byte))
    {
        value |= (uint32_t)(byte & 0x3F) << shift;
        shift += 6;
        if (!(byte & 0x40))
            break;
        if (shift >= 24)
            break;
    }
    return value;
}

static uint8_t sscr_dispatch_event(SSCR_Player *d, uint8_t byte)
{
    uint8_t group = byte & 0x40;
    uint8_t sub   = byte & 0x3F;

    if (sub == 0x3E)
        return 0;

    uint8_t note;
    if (sub == 0x3F)
    {
        if (!sscr_read_byte(d, &note))
            return 0;
    }
    else
    {
        note = sub;
    }

    if (group != 0)
    {
        uint8_t vel = 127;
        if (d->flags & 0x01)
        {
            if (!sscr_read_byte(d, &vel))
                return 0;
        }
        note = (uint8_t)((int)note - d->totalTranspose);
        NoteOnAsm(note, velocityCurve[vel]);
    }
    else
    {
        if (d->flags & 0x02)
        {
            uint8_t dummy;
            if (!sscr_read_byte(d, &dummy))
                return 0;
        }
        note = (uint8_t)((int)note - d->totalTranspose);
        NoteOffAsm(note);
    }

    return 1;
}

static void SSCR_DecodeProcess(Player *player)
{
    uint32_t now = GetSysMs();

    if (now - lastDecayMs >= 3)
    {
        GenDecayEnvlopeAsm();
        lastDecayMs = now;
    }

    SSCR_Player *d = &(player->decoder);
    if (d->status != STATUS_DECODING)
        return;

    while (d->status == STATUS_DECODING && now >= d->nextEventMs)
    {
        uint8_t byte;
        if (!sscr_peek_byte(d, &byte))
        {
            d->status = STATUS_STOP;
            break;
        }

        if (byte & 0x80)
        {
            if (!sscr_read_byte(d, &byte))
            {
                d->status = STATUS_STOP;
                break;
            }
            if (!sscr_dispatch_event(d, byte))
            {
                d->status = STATUS_STOP;
                break;
            }
        }
        else
        {
            uint32_t delta = sscr_read_delta(d);
            d->nextEventMs += (uint32_t)delta * 8;
        }
    }

    if (d->status == STATUS_STOP)
        SynthReleaseAllAsm();
}

/* ================================================================
 * SSPL 容器 + 多曲调度器
 * ================================================================ */

void PlaySchedulerNextScore(Player *player)
{
    player->scheduler.switchDirect = SCHEDULER_SCORE_NEXT;
    player->scheduler.status = SCHEDULER_SWITCHING;
    StopDecode(player);
}

void PlaySchedulerPreviousScore(Player *player)
{
    player->scheduler.switchDirect = SCHEDULER_SCORE_PREV;
    player->scheduler.status = SCHEDULER_SWITCHING;
    StopDecode(player);
}

static void SchedulerPlayIndex(Player *player, int32_t index)
{
    if (player->scheduler.maxScoreNum == 0)
    {
        player->scheduler.status = SCHEDULER_STOP;
        return;
    }

    if (index < 0)
        index = 0;
    if (index >= (int32_t)player->scheduler.maxScoreNum)
        index = player->scheduler.maxScoreNum - 1;

    player->scheduler.currentScoreIndex = index;

    uint32_t entryOff = SSPL_HEADER_SIZE + (uint32_t)index * SSPL_ENTRY_SIZE;
    uint32_t scoreOff = stream_u32(&player->scheduler.ssplStream, entryOff);

    PlayScore(player, &player->scheduler.ssplStream, scoreOff);
    player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
}

void PlaySchedulerProcess(Player *player)
{
    switch (player->scheduler.status)
    {
    case SCHEDULER_READY_TO_SWITCH:
        if (player->decoder.status != STATUS_STOP)
            break;

        if (player->scheduler.schedulerMode == MODE_SINGLE_SONG)
        {
            player->scheduler.status = SCHEDULER_STOP;
        }
        else
        {
            player->scheduler.switchDirect = SCHEDULER_SCORE_NEXT;
            player->scheduler.status = SCHEDULER_SWITCHING;
        }
        break;

    case SCHEDULER_SWITCHING:
        player->scheduler.status = player->scheduler.switchDirect;
        break;

    case SCHEDULER_SCORE_PREV:
        player->scheduler.currentScoreIndex--;
        if (player->scheduler.currentScoreIndex < 0)
            player->scheduler.currentScoreIndex = player->scheduler.maxScoreNum - 1;
        SchedulerPlayIndex(player, player->scheduler.currentScoreIndex);
        break;

    case SCHEDULER_SCORE_NEXT:
        player->scheduler.currentScoreIndex++;
        if (player->scheduler.currentScoreIndex >= (int32_t)player->scheduler.maxScoreNum)
        {
            if (player->scheduler.schedulerMode == MODE_ORDER_PLAY)
                player->scheduler.currentScoreIndex = 0;
            else
            {
                player->scheduler.status = SCHEDULER_STOP;
                player->scheduler.currentScoreIndex = -1;
                break;
            }
        }
        SchedulerPlayIndex(player, player->scheduler.currentScoreIndex);
        break;

    case SCHEDULER_SCORE_DIRECT:
        SchedulerPlayIndex(player, player->scheduler.targetScoreIndex);
        break;

    case SCHEDULER_STOP:
        break;
    }
}

/* ================================================================
 * 公开 API
 * ================================================================ */

void StartPlayScheduler(Player *player, uint8_t mode)
{
    storage_init();
    stream_init(&player->scheduler.ssplStream, storage_get_base_addr(), 0xFFFFFFFFUL);
    player->scheduler.currentScoreIndex = -1;
    player->scheduler.status = SCHEDULER_STOP;
    player->scheduler.schedulerMode = mode;

    if (stream_u8(&player->scheduler.ssplStream, 0) != 'S'
     || stream_u8(&player->scheduler.ssplStream, 1) != 'S'
     || stream_u8(&player->scheduler.ssplStream, 2) != 'P'
     || stream_u8(&player->scheduler.ssplStream, 3) != 'L')
    {
        player->scheduler.maxScoreNum = 0;
        return;
    }

    player->scheduler.maxScoreNum = stream_u16(&player->scheduler.ssplStream, 6);

    if (player->scheduler.maxScoreNum == 0)
    {
        player->scheduler.status = SCHEDULER_STOP;
        return;
    }

    player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
}

void SchedulerPlaySong(Player *player, int32_t index)
{
    player->scheduler.targetScoreIndex = index;
    player->scheduler.switchDirect = SCHEDULER_SCORE_DIRECT;
    player->scheduler.status = SCHEDULER_SWITCHING;
    StopDecode(player);
}

void StopPlayScheduler(Player *player)
{
    StopDecode(player);
    player->scheduler.status = SCHEDULER_STOP;
}

void PlayScore(Player *player, ScoreStream *sspl, uint32_t offset)
{
    if (stream_u8(sspl, offset + 0) != 'S'
     || stream_u8(sspl, offset + 1) != 'S'
     || stream_u8(sspl, offset + 2) != 'C'
     || stream_u8(sspl, offset + 3) != 'R')
    {
        player->decoder.status = STATUS_STOP;
        return;
    }

    player->decoder.flags          = stream_u8(sspl, offset + 5);
    player->decoder.totalTranspose = (int8_t)stream_u8(sspl, offset + 12);

    uint32_t dataLen = stream_u32(sspl, offset + 8);

    stream_sub(&player->decoder.stream, sspl, offset + SSCR_HEADER_SIZE, dataLen);

    lastDecayMs = GetSysMs();
    uint32_t delta = sscr_read_delta(&(player->decoder));
    player->decoder.nextEventMs = lastDecayMs + delta * 8;
    player->decoder.status = STATUS_DECODING;
}

void StopDecode(Player *player)
{
    player->decoder.status = STATUS_STOP;
    SynthReleaseAllAsm();
}

void PlayerProcess(Player *player)
{
    SSCR_DecodeProcess(player);
    PlaySchedulerProcess(player);
}

void PlayerInit(Player *player, Synthesizer *synthesizer)
{
    player->decoder.status = STATUS_STOP;
    lastDecayMs = GetSysMs();
    SynthInit(synthesizer);
}

void PlayerPlay(Player *player)
{
    if (player->scheduler.status == SCHEDULER_STOP)
    {
        player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
    }
}

void PlayerStop(Player *player)
{
    StopDecode(player);
    player->scheduler.status = SCHEDULER_STOP;
}
