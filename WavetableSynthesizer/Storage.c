//====================================================================
// Storage.c — 内置 __code FLASH 后端 (movc a,@a+dptr)
//
// 编译条件: 未定义 STORAGE_BACKEND_SPI 时启用
//====================================================================

#include "Storage.h"

#ifndef STORAGE_BACKEND_SPI

void stream_init(ScoreStream *s, uint32_t base, uint32_t size)
{
    s->data = (__code uint8_t *)(uint16_t)base;
    s->pos  = 0;
    s->size = size;
}

void stream_sub(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size)
{
    s->data = parent->data + offset;
    s->pos  = 0;
    s->size = size;
}

uint8_t stream_read(ScoreStream *s, uint8_t *out)
{
    if (s->pos >= s->size)
        return 0;
    *out = s->data[s->pos++];
    return 1;
}

uint8_t stream_peek(ScoreStream *s, uint8_t *out)
{
    if (s->pos >= s->size)
        return 0;
    *out = s->data[s->pos];
    return 1;
}

uint8_t stream_u8(ScoreStream *s, uint32_t offset)
{
    return s->data[offset];
}

uint16_t stream_u16(ScoreStream *s, uint32_t offset)
{
    return (uint16_t)s->data[offset]
         | ((uint16_t)s->data[offset + 1] << 8);
}

uint32_t stream_u32(ScoreStream *s, uint32_t offset)
{
    return (uint32_t)s->data[offset]
         | ((uint32_t)s->data[offset + 1] << 8)
         | ((uint32_t)s->data[offset + 2] << 16)
         | ((uint32_t)s->data[offset + 3] << 24);
}

#endif
