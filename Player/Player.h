//====================================================================
// Player.h — SSCR 乐谱解码器 + SSPL 多曲播放调度器
//
// 数据格式:
//   SSPL 容器 → 内含多条 SSCR 乐谱
//   每条 SSCR 乐谱有独立的 13 字节 header + 事件流
//
//   SSPL 文件结构:
//     [0-3]   "SSPL" 标识
//     [4]     Version = 0x01
//     [5]     Flags = 0x00
//     [6-7]   Count (uint16_t LE)
//     [8-11]  Reserved
//     [12..]  Entry Table (Count × 8 字节: offset(4) + size(4) LE)
//     [...]   SSCR Data 0, SSCR Data 1, ...
//
//   SSCR 文件结构:
//     [0-3]   "SSCR" 标识
//     [4]     Version
//     [5]     Flags (bit0=NoteOn有velocity, bit1=NoteOff有velocity)
//     [6-7]   TickPerSecond (uint16_t LE)
//     [8-11]  DataLength (uint32_t LE)
//     [12]    TotalTranspose (int8_t)
//     [13..]  Event Data (delta + event 流)
//
//   事件编码:
//     byte bit7=0 → delta (6-bit chunk, 小端)
//     byte bit7=1 → event:
//       0x80-0xBD: NoteOff 直连 (note = byte & 0x3F)
//       0xBE:      EndOfScore
//       0xBF:      NoteOff 扩展 (下一字节 = note)
//       0xC0-0xFD: NoteOn 直连 (note = byte & 0x3F)
//       0xFE:      保留
//       0xFF:      NoteOn 扩展 (下一字节 = note)
//
// 目前实现: 忽略 NoteOff, 忽略 velocity
//
// 串口协议:
//   完整帧协议: SYNC(0x5A) + CMD + LEN + PAYLOAD + CHECKSUM
//   详见 Protocol.h
//====================================================================

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <stdint.h>
#include "SynthCore.h"
#include "PeriodTimer.h"
#include "Storage.h"

// ======================== 常量 ========================

#define SSCR_HEADER_SIZE  13
#define SSPL_HEADER_SIZE  12
#define SSPL_ENTRY_SIZE   8

// ======================== 枚举定义 ========================

enum DECODER_STATUS
{
    STATUS_STOP = 0,
    STATUS_DECODING = 2
};

enum SCHEDULER_MODE
{
    MODE_ORDER_PLAY = 0,
    MODE_LIST_ONCE = 1,
    MODE_SINGLE_SONG = 2,
};

enum SCHEDULER_STATUS
{
    SCHEDULER_READY_TO_SWITCH = 0,
    SCHEDULER_SWITCHING,
    SCHEDULER_SCORE_PREV,
    SCHEDULER_SCORE_NEXT,
    SCHEDULER_SCORE_DIRECT,
    SCHEDULER_STOP,
};

// ======================== 结构体定义 ========================

// SSCR_Player: 单首 SSCR 乐谱解码器
typedef struct _SSCR_Player
{
    ScoreStream stream;         // 事件数据流 (base+pos+size, 跳过 13 字节 header)
    uint32_t nextEventMs;       // 下一组事件的触发时刻 (ms)
    uint8_t flags;              // SSCR header flags
    int8_t totalTranspose;      // 编码移调值 (还原: originalNote = encodedNote - totalTranspose)
    uint8_t status;             // STATUS_STOP / STATUS_DECODING
} SSCR_Player;

// PlayScheduler: 多曲调度器
typedef struct _PlayScheduler
{
    uint8_t schedulerMode;                  // 播放模式
    int32_t currentScoreIndex;             // 当前曲目编号
    uint32_t maxScoreNum;                   // 曲目总数
    ScoreStream ssplStream;                // 完整 SSPL 数据流
    uint8_t status;                         // 调度器状态
    uint8_t switchDirect;                   // 切歌方向
    int32_t targetScoreIndex;              // SET_SONG 目标索引
} PlayScheduler;

// Player: 播放器总体结构
typedef struct _Player
{
    SSCR_Player decoder;
    PlayScheduler scheduler;
} Player;


// ======================== 公有函数 ========================

extern void PlayerInit(Player *player, Synthesizer *synthesizer);
extern void PlayerProcess(Player *player);

extern void StartPlayScheduler(Player *player, uint8_t mode);
extern void StopPlayScheduler(Player *player);
extern void SchedulerPlaySong(Player *player, int32_t index);

extern void PlaySchedulerNextScore(Player *player);
extern void PlaySchedulerPreviousScore(Player *player);

extern void PlayScore(Player *player, ScoreStream *sspl, uint32_t offset);
extern void StopDecode(Player *player);

extern void PlayerPlay(Player *player);
extern void PlayerStop(Player *player);

#endif
