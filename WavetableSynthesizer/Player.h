//====================================================================
// Player.h — 乐谱解码与播放调度器
//
// 三层架构:
//   ScoreDecoder   — 按时间线从乐谱中解码出音符, 喂给 NoteOnAsm
//   PlayScheduler  — 管理多首曲目的切换 (状态机)
//   Player         — 组合上面两者
//
// 数据来源: flash 中以 "SCRE" 为魔数的 Score[] 数组,
// 内含曲目数量 + 地址表 + 每首曲目的 delta/note 数据。
//
//   Score[] 布局:
//     [0-3]   "SCRE" 标识
//     [4-7]   scoreCount (uint32_t, little-endian)
//     [8-11]  firstAddr  (uint32_t, 第一首的绝对偏移)
//     [12..]  其余曲目地址表 (scoreCount-1) × 4 字节
//     [firstAddr..] 第一首曲目数据
//
//   每首曲目数据格式 (交替 delta / note 字节):
//     byte < 0x80   — delta-time (累积到 lastScoreTick)
//     byte >= 0x80  — note-on (MIDI号 = byte & 0x7F), 同时结束本组, 下一字节为 delta
//     0xFF          — 延长标记 (与上一 delta 累加), 连续出现结束曲目
//
//   播放模式:
//     MODE_ORDER_PLAY   — 循环播放所有曲目
//     MODE_LIST_ONCE    — 全部播完一轮停止
//     MODE_SINGLE_SONG  — 只播当前选中的一首停止 (不自动切歌)
//
//   UART 命令 (115200 baud):
//     0xFE — 上一首
//     0xFD — 下一首
//     0xDD — 软件复位
//====================================================================

#ifndef __PLAYER_H__
#define __PLAYER_H__

#include <stdint.h>
#include "SynthCore.h"
#include "PeriodTimer.h"

// ======================== 枚举定义 ========================

// 解码器状态
enum DECODER_STATUS
{
    STATUS_STOP = 0,       // 解码器空闲, 无曲目在播放
    STATUS_DECODING = 2    // 解码器活跃, 正在处理乐谱
};

// 播放模式 — 决定一首播完后自动行为
enum SCHEDULER_MODE
{
    MODE_ORDER_PLAY = 0,   // 循环无限播放
    MODE_LIST_ONCE = 1,    // 全部曲目播完一轮停止
    MODE_SINGLE_SONG = 2,  // 只播当前选中曲目, 播完停止 (UART 仍可手动切歌)
};

// 调度器状态机
//
//   状态迁移:
//                               START
//                                 │
//                                 ▼
//     ┌────────────────→ READY_TO_SWITCH ──(曲终 + auto模式)──→ SWITCHING
//     │                        │                                   │
//     │              (UART命令 + status→SWITCHING)                  │
//     │                        │                                   ▼
//     │                  ┌─────┘                            SCORE_PREV / NEXT
//     │                  ▼                                       │
//     │             SWITCHING  ◄─────────────────────────────────┘
//     │                  │
//     │                  ▼
//     └── play ─── PREV / NEXT ──(越界)──→ STOP (idle)
//
enum SCHEDULER_STATUS
{
    SCHEDULER_READY_TO_SWITCH = 0,  // 空闲等待
    SCHEDULER_SWITCHING,             // 中间过渡 (一帧)
    SCHEDULER_SCORE_PREV,            // 切换到上一首
    SCHEDULER_SCORE_NEXT,            // 切换到下一首
    SCHEDULER_STOP,                  // 停止 (进入 IDLE 低功耗)
};

// ======================== 结构体定义 ========================

// ScoreListHeader: flash 中 Score[] 起始处的文件头 (12 字节)
typedef struct _ScoreListHeader
{
    char identifier[4];       // "SCRE" 魔数
    uint32_t scoreCount;      // 曲目总数 (little-endian)
    uint32_t firstAddr;       // 第一首曲目相对偏移
} ScoreListHeader;

// ScoreDecoder: 单首曲目解码器 (8 字节)
//   状态机: STOP ↔ DECODING (PlayScore 进入, 遇到 0xFF 或 StopDecode 退出)
typedef struct _ScoreDecoder
{
    uint32_t lastScoreTick;          // 下一音符触发时刻 (= currentTick>>8)
    uint8_t  status;                 // STATUS_STOP / STATUS_DECODING
    __code uint8_t *scorePointer;   // flash 中乐谱当前读位置 (3 字节指针)
} ScoreDecoder;

// PlayScheduler: 多曲调度器 (14 字节)
//
//   currentScoreIndex:
//     -1 — 初始, 尚未播放任何曲目 (首次 SCORE_NEXT 时 +1 → 0)
//     0..maxScoreNum-1 — 当前播放的曲目编号
//     maxScoreNum — 越界 (仅在 LIST_ONCE/ORDER 自动切歌时出现, 下一秒纠正)
typedef struct _PlayScheduler
{
    uint8_t  schedulerMode;                  // MODE_ORDER_PLAY / MODE_LIST_ONCE / MODE_SINGLE_SONG
    int32_t  currentScoreIndex;             // 当前曲目编号
    uint32_t maxScoreNum;                    // 曲目总数
    __code ScoreListHeader *scoreListHeader; // SCRE 头部指针 (3 字节)
    uint8_t  status;                         // 调度器状态
    uint8_t  switchDirect;                   // 切歌方向 (SCORE_PREV / SCORE_NEXT)
} PlayScheduler;

// Player: 播放器总体结构 (22 字节, 分配在 XRAM)
typedef struct _Player
{
    ScoreDecoder decoder;
    PlayScheduler scheduler;
} Player;


// ======================== 公有函数 ========================

// ---- 生命周期 ----
extern void PlayerInit(Player *player, Synthesizer *synthesizer);
extern void PlayerProcess(Player *player);  // 主循环每帧调用

// ---- 调度器控制 ----
// 启动调度器, mode 决定播完行为:
//   MODE_ORDER_PLAY  — 循环
//   MODE_LIST_ONCE   — 全部播完停止
//   MODE_SINGLE_SONG — 只播当前曲目, 播完停止 (UART 可手动切歌)
extern void StartPlayScheduler(Player *player, uint8_t mode);

// 停止调度器 (停止解码 → 进入 STOP 状态)
extern void StopPlayScheduler(Player *player);

// 重置并开始播放当前 index 指向的曲目 (index 自动钳位到有效范围)
//   不管当前处于什么状态, 强行切到指定曲目
extern void SchedulerPlaySong(Player *player, int32_t index);

// ---- UART 命令回调 (ISR 中调用) ----
extern void PlaySchedulerNextScore(Player *player);
extern void PlaySchedulerPreviousScore(Player *player);

// ---- 内部函数 (也可被外部调用) ----
extern void PlayScore(Player *player, __code uint8_t *score);
extern void StopDecode(Player *player);

#endif
