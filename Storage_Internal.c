#include "Storage.h"

#ifndef STORAGE_BACKEND_SPI

typedef struct
{
	__code uint8_t *data;
	uint32_t pos;
	uint32_t size;
} StreamImpl;

__code extern unsigned char Score[];

void storage_init(void)
{
}

uint32_t storage_get_base_addr(void)
{
	return (uint32_t)(uint16_t)Score;
}

void stream_init(ScoreStream *s, uint32_t base, uint32_t size)
{
	StreamImpl *si = (StreamImpl *)s;
	si->data = (__code uint8_t *)(uint16_t)base;
	si->pos  = 0;
	si->size = size;
}

void stream_sub(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size)
{
	StreamImpl *si = (StreamImpl *)s;
	StreamImpl *pi = (StreamImpl *)parent;
	si->data = pi->data + offset;
	si->pos  = 0;
	si->size = size;
}

uint8_t stream_read(ScoreStream *s, uint8_t *out)
{
	StreamImpl *si = (StreamImpl *)s;
	if (si->pos >= si->size)
		return 0;
	*out = si->data[si->pos++];
	return 1;
}

uint8_t stream_peek(ScoreStream *s, uint8_t *out)
{
	StreamImpl *si = (StreamImpl *)s;
	if (si->pos >= si->size)
		return 0;
	*out = si->data[si->pos];
	return 1;
}

uint8_t stream_u8(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return si->data[offset];
}

uint16_t stream_u16(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return (uint16_t)si->data[offset]
	     | ((uint16_t)si->data[offset + 1] << 8);
}

uint32_t stream_u32(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return (uint32_t)si->data[offset]
	     | ((uint32_t)si->data[offset + 1] << 8)
	     | ((uint32_t)si->data[offset + 2] << 16)
	     | ((uint32_t)si->data[offset + 3] << 24);
}

#endif
