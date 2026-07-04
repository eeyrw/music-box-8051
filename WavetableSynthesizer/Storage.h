//====================================================================
// Storage.h — 乐谱存储抽象层
//
// 两种后端，编译时切换:
//   默认 (无定义)        — 内置 __code FLASH (movc a,@a+dptr)
//   STORAGE_BACKEND_SPI — 外部 SPI FLASH (W25Qxx 等)
//
// ScoreStream 封装一条字节流: base + pos + size
//   stream_read / stream_peek — 流式逐字节读取 (事件解码器热路径)
//   stream_u8 / stream_u16 / stream_u32 — 随机偏移读取 (header 解析)
//
// 使用方式:
//   make                     → 内置 FLASH
//   make STORAGE=spi         → SPI FLASH, 默认 P2.2~P2.5
//====================================================================

#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <stdint.h>

// ======================== 后端选择 ========================

#ifndef STORAGE_BACKEND_SPI

// ---- 内置 __code FLASH ----
typedef struct _ScoreStream
{
    __code uint8_t *data;
    uint32_t pos;
    uint32_t size;
} ScoreStream;

#else

// ---- 外部 SPI FLASH ----
typedef struct _ScoreStream
{
    uint32_t base_addr;
    uint32_t pos;
    uint32_t size;
} ScoreStream;

#endif

// ======================== 公共 API ========================

// 初始化根流。base: 内置闪存用指针地址，SPI 闪存用绝对字节地址。
void stream_init(ScoreStream *s, uint32_t base, uint32_t size);

// 从父流打开子流 (offset 为相对父流的偏移)
void stream_sub(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size);

// 流式逐字节读取 (推进 s->pos)
uint8_t stream_read(ScoreStream *s, uint8_t *out);
uint8_t stream_peek(ScoreStream *s, uint8_t *out);

// 随机偏移读取 (不修改 s->pos)
uint8_t  stream_u8(ScoreStream *s, uint32_t offset);
uint16_t stream_u16(ScoreStream *s, uint32_t offset);
uint32_t stream_u32(ScoreStream *s, uint32_t offset);

#ifdef STORAGE_BACKEND_SPI
// SPI FLASH 初始化 (配置硬件 SPI, 在 StartPlayScheduler 之前调用)
void spi_storage_init(void);
#endif

#endif
