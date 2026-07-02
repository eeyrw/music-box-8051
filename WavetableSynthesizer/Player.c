//====================================================================
// Player.c — 乐谱解码器 + 多曲播放调度器
//
// 调用链:
//   main() → PlayerProcess()
//              ├─ ScoreDecodeProcess()   — 解码当前曲, 触发音符
//              └─ PlaySchedulerProcess() — 管理曲目切换
//
//   播放模式 (SCHEDULER_MODE) 决定一首曲目播完后的自动行为:
//     MODE_ORDER_PLAY  — 自动切下一首, 到头循环
//     MODE_LIST_ONCE   — 自动切下一首, 全部播完停止
//     MODE_SINGLE_SONG — 不自动切歌, 播完停止
//
//   UART 0xFE/0xFD 为手动切歌 (ISR 触发), 不受模式限制。
//
//   关键全局变量:
//     _currentTick    (DSEG 0x11, uint32_t) — ISR 递增, >>8 后用于节拍比较
//     _decayGenTick   (DSEG 0x10, uint8_t)  — 包络衰减计数器
//     _ScoreDataListPtr (XRAM)              — 指向 Score[] 的 SCRE 头部
//====================================================================

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "SynthCore.h"
#include "Player.h"
#include "RegisterDefine.h"
#include "Bsp.h"

__code extern unsigned char Score[];

__code ScoreListHeader *ScoreDataListPtr = (__code ScoreListHeader *)Score;

//====================================================================
// UpdateNextScoreTick — 从乐谱读取下一段 delta-time
//
// 不断读取乐谱字节并累加到 lastScoreTick, 直到遇到非 0xFF 的字节。
// 0xFF 是延长标记 — 与上一个 delta 累加, 用于超长间隔。
//====================================================================
void UpdateNextScoreTick(ScoreDecoder *decoder)
{
    uint32_t tempU32 = decoder->lastScoreTick;
    uint8_t temp;
    do
    {
        temp = *(decoder->scorePointer++);
        tempU32 += temp;
    } while (temp == 0xFF);
    decoder->lastScoreTick = tempU32;
}

//====================================================================
// ScoreDecodeProcess — 单曲乐谱解码
//
// 每主循环迭代执行一次。负责:
//   1. 定时触发包络衰减 (decayGenTick >= DECAY_TIME_FACTOR)
//   2. 在节拍到达时从乐谱中读取音符, 调用 NoteOnAsm
//
// 乐谱解码循环:
//   while ((temp & 0x80) == 0)
//     连续的无 bit7 字节视为同一和弦的多个音符;
//     遇到 bit7==1 的字节结束本组 (该字节仍作为音符播放, 值 = byte & 0x7F)。
//
//   特殊: 0xFF → 曲终标记, 置 status = STATUS_STOP
//====================================================================
void ScoreDecodeProcess(Player *player)
{
    uint8_t temp;

    // ---- 包络衰减定时: 每 100 tick 推进一格 ----
    if (decayGenTick >= DECAY_TIME_FACTOR)
    {
        GenDecayEnvlopeAsm();
        decayGenTick = 0;
    }

    if (player->decoder.status != STATUS_DECODING)
        return;

    // 关中断保护 currentTick 的 32-bit 读取
    uint32_t tmp;
    EA = 0;
    tmp = (currentTick >> 8);
    EA = 1;

    if (tmp < player->decoder.lastScoreTick)
        return;  // 时间未到

    // 时间到 — 处理本组所有音符
    do
    {
        temp = *(player->decoder.scorePointer++);
        if (temp == 0xFF)
            player->decoder.status = STATUS_STOP;
        else
            NoteOnAsm(temp);
    } while ((temp & 0x80) == 0);

    // 如果处理完本组后曲未终, 读取下一组的 delta-time
    if (player->decoder.status == STATUS_DECODING)
        UpdateNextScoreTick(&(player->decoder));
}

//====================================================================
// 手动切歌: UART ISR 回调
//
// 不在 ISR 中直接切歌, 而是标记切换方向后留到主循环消费。
// 这样可以避免 ISR 中的重入问题 (ISR 调用的函数可能与主循环共享状态)。
//====================================================================
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

//====================================================================
// GetScorePhysicalAddr — 曲目偏移地址 → flash 绝对指针
//====================================================================
__code uint8_t *GetScorePhysicalAddr(uint32_t addr)
{
    return (__code uint8_t *)((__code uint8_t *)ScoreDataListPtr + addr);
}

//====================================================================
// PlaySchedulerProcess — 多曲调度状态机 (每主循环迭代调用一次)
//
// 状态图:
//
//   READY_TO_SWITCH ──(曲终 && mode != SINGLE)──→ SWITCHING
//         ↑                                             │
//         │            ┌────────────────────────────────┘
//         │            ▼
//         │     SCORE_PREV / NEXT
//         │            │
//         │       ┌────┴────选曲────┐
//         │       ▼                ▼         越界
//         │   [有效范围内]    [越界] ──→ STOP
//         │       │                │
//         │    PlayScore      (mode==SINGLE → STOP)
//         │       │           (mode!=SINGLE → 纠正index → PlayScore)
//         │       ▼                │
//         └── READY_TO_SWITCH ◄────┘
//
//   UART 0xFE/0xFD ──→ 标记 switchDirect → SWITCHING
//
//   STOP ──→ IntoPowerDown (IDLE, 空闲状态可被 ISR 唤醒)
//
// 模式对自动切歌的影响:
//   READY_TO_SWITCH 在曲终时:
//     ORDER_PLAY / LIST_ONCE  → 自动触发 SCORE_NEXT
//     SINGLE_SONG             → 不触发, 保持 READY (即进入 STOP by default...
//                                hmm wait, we need SINGLE to go to STOP)
//
// 我需要在 READY_TO_SWITCH 的曲终处理中加入 SINGLE 判断。
//====================================================================
void PlaySchedulerProcess(Player *player)
{
    __code uint32_t *ScoreList = (__code uint32_t *)(&(player->scheduler.scoreListHeader)->firstAddr);

    switch (player->scheduler.status)
    {
    // ---- 空闲: 检测曲终 → 按模式决定是否自动切歌 ----
    case SCHEDULER_READY_TO_SWITCH:
        if (player->decoder.status != STATUS_STOP)
            break;

        // 曲已播完
        currentTick = 0;
        decayGenTick = 0;

        if (player->scheduler.schedulerMode == MODE_SINGLE_SONG)
        {
            // 单曲模式: 不自动切歌, 直接停止
            player->scheduler.status = SCHEDULER_STOP;
        }
        else
        {
            // ORDER 或 LIST_ONCE: 自动切下一首
            player->scheduler.switchDirect = SCHEDULER_SCORE_NEXT;
            player->scheduler.status = SCHEDULER_SWITCHING;
        }
        break;

    // ---- 过渡态: 应用切歌方向 ----
    case SCHEDULER_SWITCHING:
        player->scheduler.status = player->scheduler.switchDirect;
        break;

    // ---- 切到上一首 ----
    case SCHEDULER_SCORE_PREV:
        player->scheduler.currentScoreIndex--;
        if (player->scheduler.currentScoreIndex < 0)
            player->scheduler.currentScoreIndex = player->scheduler.maxScoreNum - 1;
        PlayScore(player, GetScorePhysicalAddr(ScoreList[player->scheduler.currentScoreIndex]));
        player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
        break;

    // ---- 切到下一首 ----
    case SCHEDULER_SCORE_NEXT:
        player->scheduler.currentScoreIndex++;
        if (player->scheduler.currentScoreIndex >= (int32_t)player->scheduler.maxScoreNum)
        {
            // 超过列表末尾
            if (player->scheduler.schedulerMode == MODE_ORDER_PLAY)
            {
                // 循环 → 回绕到第一首
                player->scheduler.currentScoreIndex = 0;
                PlayScore(player, GetScorePhysicalAddr(ScoreList[0]));
                player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
            }
            else
            {
                // LIST_ONCE / SINGLE → 停止
                player->scheduler.status = SCHEDULER_STOP;
                player->scheduler.currentScoreIndex = -1;
            }
        }
        else
        {
            // 有效范围内 → 播放
            PlayScore(player, GetScorePhysicalAddr(ScoreList[player->scheduler.currentScoreIndex]));
            player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
        }
        break;

    // ---- 停止: 进入 IDLE 低功耗 ----
    case SCHEDULER_STOP:
        IntoPowerDown();
        break;
    }
}

//====================================================================
//====================================================================
// StartPlayScheduler — 初始化调度器
//
// 校验 ScoreDataListPtr 是否指向有效 "SCRE" 头部。
// 通过后根据 mode 设置调度器行为:
//   MODE_ORDER_PLAY  → 循环无限播放
//   MODE_LIST_ONCE   → 全部播完停止
//   MODE_SINGLE_SONG → 只播一首停止
//====================================================================
void StartPlayScheduler(Player *player, uint8_t mode)
{
    // 所有字段先设默认值
    player->scheduler.scoreListHeader = ScoreDataListPtr;
    player->scheduler.currentScoreIndex = -1;
    player->scheduler.status = SCHEDULER_STOP;
    player->scheduler.schedulerMode = mode;

    // 校验 SCRE 头部
    if (ScoreDataListPtr->identifier[0] != 'S' ||
        ScoreDataListPtr->identifier[1] != 'C' ||
        ScoreDataListPtr->identifier[2] != 'R' ||
        ScoreDataListPtr->identifier[3] != 'E' ||
        ScoreDataListPtr->scoreCount == 0)
    {
        player->scheduler.maxScoreNum = 0;
        return;  // 校验失败, 保持 STOP
    }

    player->scheduler.maxScoreNum = ScoreDataListPtr->scoreCount;
    player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
}

//====================================================================
// SchedulerPlaySong — 播放指定曲目
//
// 不管当前状态, 直接停止解码并跳到指定曲目。
// index 钳位到 [0, maxScoreNum-1]; 若列表为空则进入 STOP。
// 这是为外部调用者提供的"一键播放"接口。
//====================================================================
void SchedulerPlaySong(Player *player, int32_t index)
{
    if (player->scheduler.maxScoreNum == 0)
    {
        player->scheduler.status = SCHEDULER_STOP;
        return;
    }

    // 钳位
    if (index < 0)
        index = 0;
    if (index >= (int32_t)player->scheduler.maxScoreNum)
        index = player->scheduler.maxScoreNum - 1;

    player->scheduler.currentScoreIndex = index;
    StopDecode(player);

    __code uint32_t *ScoreList = (__code uint32_t *)(&(player->scheduler.scoreListHeader)->firstAddr);
    PlayScore(player, GetScorePhysicalAddr(ScoreList[index]));
    player->scheduler.status = SCHEDULER_READY_TO_SWITCH;
}

//====================================================================
// StopPlayScheduler — 停止调度器
//====================================================================
void StopPlayScheduler(Player *player)
{
    StopDecode(player);
    player->scheduler.status = SCHEDULER_STOP;
}

//====================================================================
// PlayScore — 开始播放一首曲目
//
// 重置节拍和计数器, 设置乐谱指针, 预读第一个 delta-time,
// 激活解码器。
//====================================================================
void PlayScore(Player *player, __code uint8_t *score)
{
    currentTick = 0;
    decayGenTick = 0;
    player->decoder.lastScoreTick = 0;
    player->decoder.scorePointer = score;
    UpdateNextScoreTick(&(player->decoder));  // 预读第一个 delta
    player->decoder.status = STATUS_DECODING;
}

//====================================================================
// StopDecode — 停止解码
//
// 不清除已发声的声道 — 让它们自然衰减 (与新曲目形成语音重叠)。
//====================================================================
void StopDecode(Player *player)
{
    player->decoder.status = STATUS_STOP;
    currentTick = 0;
    decayGenTick = 0;
    player->decoder.lastScoreTick = 0;
}

//====================================================================
// PlayerProcess — 主循环入口
//
// 先解码乐谱 (可能触发音符、标记曲终),
// 然后调度器根据 decoder.status 和当前模式决定是否切歌。
//====================================================================
void PlayerProcess(Player *player)
{
    ScoreDecodeProcess(player);
    PlaySchedulerProcess(player);
}

//====================================================================
// PlayerInit — 初始化播放器和关联的 Synthesizer
//====================================================================
void PlayerInit(Player *player, Synthesizer *synthesizer)
{
    player->decoder.status = STATUS_STOP;
    currentTick = 0;
    decayGenTick = 0;
    player->decoder.lastScoreTick = 0;
    player->decoder.scorePointer = NULL;
    SynthInit(synthesizer);
}
