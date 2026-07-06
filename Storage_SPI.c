#include "Storage.h"
#include "SpiFlash.h"

typedef struct
{
	uint32_t base_addr;
	uint32_t pos;
	uint32_t size;
} StreamImpl;

void storage_init_spi(void)
{
	SpiFlash_Init();
}

uint32_t storage_get_base_addr_spi(void)
{
	return SPI_FLASH_BASE_ADDR;
}

void stream_init_spi(ScoreStream *s, uint32_t base, uint32_t size)
{
	StreamImpl *si = (StreamImpl *)s;
	si->base_addr = base;
	si->pos       = 0;
	si->size      = size;
}

void stream_sub_spi(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size)
{
	StreamImpl *si = (StreamImpl *)s;
	StreamImpl *pi = (StreamImpl *)parent;
	si->base_addr = pi->base_addr + offset;
	si->pos       = 0;
	si->size      = size;
}

uint8_t stream_read_spi(ScoreStream *s, uint8_t *out)
{
	StreamImpl *si = (StreamImpl *)s;
	if (si->pos >= si->size)
		return 0;
	*out = SpiFlash_ReadCached(si->base_addr + si->pos);
	si->pos++;
	return 1;
}

uint8_t stream_peek_spi(ScoreStream *s, uint8_t *out)
{
	StreamImpl *si = (StreamImpl *)s;
	if (si->pos >= si->size)
		return 0;
	*out = SpiFlash_ReadCached(si->base_addr + si->pos);
	return 1;
}

uint8_t stream_u8_spi(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	return SpiFlash_ReadCached(si->base_addr + offset);
}

uint16_t stream_u16_spi(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	uint8_t buf[2];
	SpiFlash_ReadBytesCached(si->base_addr + offset, buf, 2);
	return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

uint32_t stream_u32_spi(ScoreStream *s, uint32_t offset)
{
	StreamImpl *si = (StreamImpl *)s;
	uint8_t buf[4];
	SpiFlash_ReadBytesCached(si->base_addr + offset, buf, 4);
	return (uint32_t)buf[0]
	     | ((uint32_t)buf[1] << 8)
	     | ((uint32_t)buf[2] << 16)
	     | ((uint32_t)buf[3] << 24);
}
