#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <stdint.h>

#define STREAM_IMPL_SIZE  12

enum STORAGE_BACKEND
{
	STORAGE_BACKEND_INTERNAL = 0,
	STORAGE_BACKEND_SPI      = 1,
};

typedef struct _ScoreStream
{
	uint8_t _[STREAM_IMPL_SIZE];
} ScoreStream;

void storage_select_backend(uint8_t type);
uint8_t storage_get_backend(void);
void storage_auto_detect(void);

void storage_init(void);
uint32_t storage_get_base_addr(void);

void stream_init(ScoreStream *s, uint32_t base, uint32_t size);
void stream_sub(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size);
uint8_t stream_read(ScoreStream *s, uint8_t *out);
uint8_t stream_peek(ScoreStream *s, uint8_t *out);
uint8_t  stream_u8(ScoreStream *s, uint32_t offset);
uint16_t stream_u16(ScoreStream *s, uint32_t offset);
uint32_t stream_u32(ScoreStream *s, uint32_t offset);

#endif
