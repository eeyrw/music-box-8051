//====================================================================
// Player.c — SSCR 乐谱解码器 + SSPL 多曲播放调度器
//
// 数据流:
//   Score[] (SSPL 容器)
//     → SSPL header 解析 (入口表)
//       → PlayScore(sscrScore) 初始化 SSCR 解码器
//         → SSCR_DecodeProcess() 逐 tick 解码事件流
//           → NoteOnAsm(note) 触发音符
//
// 调度时序:
//   currentTick (32-bit, ISR 递增) → >>8 得 scoreTick
//   SSCR delta 值 (raw tick) 直接与 scoreTick 比较
//   (对应 TickPerSecond=125, 即 32000/125=256=2^8)
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

__code extern unsigned char Score[];

/* ================================================================
 * SSCR 解码器内部
 * ================================================================ */

static uint8_t sscr_read_byte(SSCR_Player *d, uint8_t *out)
{
    if (d->position >= d->length)
        return 0;
    *out = d->data[d->position++];
    return 1;
}

static uint8_t sscr_peek_byte(SSCR_Player *d, uint8_t *out)
{
    if (d->position >= d->length)
        return 0;
    *out = d->data[d->position];
    return 1;
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

    // 0xBE (EOS) / 0xFE (Reserved) → 停止
    if (sub == 0x3E)
        return 0;

    uint8_t note;
    if (sub == 0x3F)
    {
        // 0xBF / 0xFF: 扩展模式 → 下一字节为完整 note
        if (!sscr_read_byte(d, &note))
            return 0;
    }
    else
    {
        // 直连模式 → note 在 sub 字段中 (0~61)
        note = sub;
    }

    if (group != 0)
    {
        // ---- NoteOn ----
        if (d->flags & 0x01)
        {
            uint8_t vel;
            if (!sscr_read_byte(d, &vel))
                return 0;
        }
        note = (uint8_t)((int)note - d->totalTranspose);
        NoteOnAsm(note);
    }
    else
    {
        // ---- NoteOff → 跳过 ----
        if (d->flags & 0x02)
        {
            uint8_t dummy;
            if (!sscr_read_byte(d, &dummy))
                return 0;
        }
    }

    return 1;
}

static void SSCR_DecodeProcess(Player *player)
{
    // 包络衰减定时
    if (decayGenTick >= DECAY_TIME_FACTOR)
    {
        GenDecayEnvlopeAsm();
        decayGenTick = 0;
    }

    SSCR_Player *d = &(player->decoder);
    if (d->status != STATUS_DECODING)
        return;

    // 关中断保护 32-bit currentTick 读取
    uint32_t tmp;
    EA = 0;
    tmp = (currentTick >> 8);
    EA = 1;

    // 处理所有到期事件
    while (d->status == STATUS_DECODING && tmp >= d->nextEventTick)
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
            d->nextEventTick += delta;
        }
    }
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

    // 从 SSPL 入口表读取该曲目的偏移
    __code uint8_t *p = player->scheduler.ssplData
                      + SSPL_HEADER_SIZE
                      + (uint16_t)((uint16_t)index * SSPL_ENTRY_SIZE);

    uint32_t offset = (uint32_t)p[0]
                    | ((uint32_t)p[1] << 8)
                    | ((uint32_t)p[2] << 16)
                    | ((uint32_t)p[3] << 24);

    PlayScore(player, (__code uint8_t *)(player->scheduler.ssplData + offset));
    player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
}

void PlaySchedulerProcess(Player *player)
{
    switch (player->scheduler.status)
    {
    case SCHEDULER_READY_TO_SWITCH:
        if (player->decoder.status != STATUS_STOP)
            break;

        currentTick = 0;
        decayGenTick = 0;

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

    case SCHEDULER_STOP:
        IntoPowerDown();
        break;
    }
}

/* ================================================================
 * 公开 API
 * ================================================================ */

void StartPlayScheduler(Player *player, uint8_t mode)
{
    player->scheduler.ssplData = Score;
    player->scheduler.currentScoreIndex = -1;
    player->scheduler.status = SCHEDULER_STOP;
    player->scheduler.schedulerMode = mode;

    // 校验 SSPL magic
    if (Score[0] != 'S' || Score[1] != 'S' ||
        Score[2] != 'P' || Score[3] != 'L')
    {
        player->scheduler.maxScoreNum = 0;
        return;
    }

    player->scheduler.maxScoreNum = (uint16_t)Score[6]
                                  | ((uint16_t)Score[7] << 8);

    if (player->scheduler.maxScoreNum == 0)
    {
        player->scheduler.status = SCHEDULER_STOP;
        return;
    }

    player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
}

void SchedulerPlaySong(Player *player, int32_t index)
{
    SchedulerPlayIndex(player, index);
}

void StopPlayScheduler(Player *player)
{
    StopDecode(player);
    player->scheduler.status = SCHEDULER_STOP;
}

void PlayScore(Player *player, __code uint8_t *score)
{
    // 校验 SSCR magic
    if (score[0] != 'S' || score[1] != 'S' ||
        score[2] != 'C' || score[3] != 'R')
    {
        player->decoder.status = STATUS_STOP;
        return;
    }

    player->decoder.flags = score[5];
    player->decoder.totalTranspose = (int8_t)score[12];

    uint32_t dataLen = (uint32_t)score[8]
                     | ((uint32_t)score[9]  << 8)
                     | ((uint32_t)score[10] << 16)
                     | ((uint32_t)score[11] << 24);

    player->decoder.data     = score + SSCR_HEADER_SIZE;
    player->decoder.length   = dataLen;
    player->decoder.position = 0;

    currentTick = 0;
    decayGenTick = 0;

    player->decoder.nextEventTick = sscr_read_delta(&(player->decoder));
    player->decoder.status = STATUS_DECODING;
}

void StopDecode(Player *player)
{
    player->decoder.status = STATUS_STOP;
    currentTick = 0;
    decayGenTick = 0;
}

void PlayerProcess(Player *player)
{
    SSCR_DecodeProcess(player);
    PlaySchedulerProcess(player);
}

void PlayerInit(Player *player, Synthesizer *synthesizer)
{
    player->decoder.status = STATUS_STOP;
    currentTick = 0;
    decayGenTick = 0;
    SynthInit(synthesizer);
}
