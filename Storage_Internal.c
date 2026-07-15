#include "Storage.h"
#include "Platform.h"

typedef struct
{
	MEM_CODE(uint8_t) *data;
	uint32_t pos;
	uint32_t size;
} StreamImpl;

extern MEM_CODE(unsigned char) Score[];

void storage_init_internal(void)
{
}

uint32_t storage_get_base_addr_internal(void)
{
	return (uint32_t)(uint16_t)Score;
}

void stream_init_internal(ScoreStream *s, uint32_t base, uint32_t size)
{
	StreamImpl *si = (StreamImpl *)s;
	si->data = (MEM_CODE(uint8_t) *)(uint16_t)base;
	si->pos  = 0;
	si->size = size;
}

void stream_sub_internal(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size)
{
	StreamImpl *si = (StreamImpl *)s;
	StreamImpl *pi = (StreamImpl *)parent;
	si->data = pi->data + offset;
	si->pos  = 0;
	si->size = size;
}

uint8_t stream_read_internal(ScoreStream *s, uint8_t *out)
{
	StreamImpl *si = (StreamImpl *)s;
	if (si->pos >= si->size)
		return 0;
	*out = si->data[si->pos++];
	return 1;
}

uint8_t stream_peek_internal(ScoreStream *s, uint8_t *out)
{
	StreamImpl *si = (StreamImpl *)s;
	if (si->pos >= si->size)
		return 0;
	*out = si->data[si->pos];
	return 1;
}

uint8_t stream_u8_internal(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return si->data[offset];
}

uint16_t stream_u16_internal(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return (uint16_t)si->data[offset]
	     | ((uint16_t)si->data[offset + 1] << 8);
}

uint32_t stream_u32_internal(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return (uint32_t)si->data[offset]
	     | ((uint32_t)si->data[offset + 1] << 8)
	     | ((uint32_t)si->data[offset + 2] << 16)
	     | ((uint32_t)si->data[offset + 3] << 24);
}
