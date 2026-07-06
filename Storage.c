#include "Storage.h"
#include "SpiFlash.h"

void storage_init_internal(void);
uint32_t storage_get_base_addr_internal(void);
void stream_init_internal(ScoreStream *s, uint32_t base, uint32_t size);
void stream_sub_internal(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size);
uint8_t stream_read_internal(ScoreStream *s, uint8_t *out);
uint8_t stream_peek_internal(ScoreStream *s, uint8_t *out);
uint8_t stream_u8_internal(ScoreStream *s, uint32_t offset);
uint16_t stream_u16_internal(ScoreStream *s, uint32_t offset);
uint32_t stream_u32_internal(ScoreStream *s, uint32_t offset);

void storage_init_spi(void);
uint32_t storage_get_base_addr_spi(void);
void stream_init_spi(ScoreStream *s, uint32_t base, uint32_t size);
void stream_sub_spi(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size);
uint8_t stream_read_spi(ScoreStream *s, uint8_t *out);
uint8_t stream_peek_spi(ScoreStream *s, uint8_t *out);
uint8_t stream_u8_spi(ScoreStream *s, uint32_t offset);
uint16_t stream_u16_spi(ScoreStream *s, uint32_t offset);
uint32_t stream_u32_spi(ScoreStream *s, uint32_t offset);

static struct
{
	void      (*init)(void);
	uint32_t  (*get_base_addr)(void);
	void      (*stream_init)(ScoreStream *, uint32_t, uint32_t);
	void      (*stream_sub)(ScoreStream *, ScoreStream *, uint32_t, uint32_t);
	uint8_t   (*stream_read)(ScoreStream *, uint8_t *);
	uint8_t   (*stream_peek)(ScoreStream *, uint8_t *);
	uint8_t   (*stream_u8)(ScoreStream *, uint32_t);
	uint16_t  (*stream_u16)(ScoreStream *, uint32_t);
	uint32_t  (*stream_u32)(ScoreStream *, uint32_t);
} storage_ops;

static uint8_t backend_type = STORAGE_BACKEND_INTERNAL;

void storage_select_backend(uint8_t type)
{
	backend_type = type;
	if (type == STORAGE_BACKEND_SPI)
	{
		storage_ops.init          = storage_init_spi;
		storage_ops.get_base_addr = storage_get_base_addr_spi;
		storage_ops.stream_init   = stream_init_spi;
		storage_ops.stream_sub    = stream_sub_spi;
		storage_ops.stream_read   = stream_read_spi;
		storage_ops.stream_peek   = stream_peek_spi;
		storage_ops.stream_u8     = stream_u8_spi;
		storage_ops.stream_u16    = stream_u16_spi;
		storage_ops.stream_u32    = stream_u32_spi;
	}
	else
	{
		storage_ops.init          = storage_init_internal;
		storage_ops.get_base_addr = storage_get_base_addr_internal;
		storage_ops.stream_init   = stream_init_internal;
		storage_ops.stream_sub    = stream_sub_internal;
		storage_ops.stream_read   = stream_read_internal;
		storage_ops.stream_peek   = stream_peek_internal;
		storage_ops.stream_u8     = stream_u8_internal;
		storage_ops.stream_u16    = stream_u16_internal;
		storage_ops.stream_u32    = stream_u32_internal;
	}
}

uint8_t storage_get_backend(void)
{
	return backend_type;
}

void storage_auto_detect(void)
{
	uint8_t jedec[3];

	SpiFlash_Init();
	SpiFlash_ReadJedecId(jedec);

	if (jedec[0] != 0xFF && jedec[0] != 0x00)
		storage_select_backend(STORAGE_BACKEND_SPI);
	else
		storage_select_backend(STORAGE_BACKEND_INTERNAL);
}

void storage_init(void)
{
	storage_ops.init();
}

uint32_t storage_get_base_addr(void)
{
	return storage_ops.get_base_addr();
}

void stream_init(ScoreStream *s, uint32_t base, uint32_t size)
{
	storage_ops.stream_init(s, base, size);
}

void stream_sub(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size)
{
	storage_ops.stream_sub(s, parent, offset, size);
}

uint8_t stream_read(ScoreStream *s, uint8_t *out)
{
	return storage_ops.stream_read(s, out);
}

uint8_t stream_peek(ScoreStream *s, uint8_t *out)
{
	return storage_ops.stream_peek(s, out);
}

uint8_t stream_u8(ScoreStream *s, uint32_t offset)
{
	return storage_ops.stream_u8(s, offset);
}

uint16_t stream_u16(ScoreStream *s, uint32_t offset)
{
	return storage_ops.stream_u16(s, offset);
}

uint32_t stream_u32(ScoreStream *s, uint32_t offset)
{
	return storage_ops.stream_u32(s, offset);
}
