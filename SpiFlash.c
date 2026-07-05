#include "SpiFlash.h"
#include "RegisterDefine.h"
#include "Bsp.h"

#define CACHE_SIZE  1024
#define CACHE_MASK   (CACHE_SIZE - 1)

#define SPI_CS       P35
#define SPI_SCLK     P32
#define SPI_MOSI     P33
#define SPI_MISO     P34

#define SPI_CS_LOW()   (SPI_CS = 0)
#define SPI_CS_HIGH()  (SPI_CS = 1)

#define CMD_READ          0x03
#define CMD_WRITE_ENABLE  0x06
#define CMD_READ_STATUS   0x05
#define CMD_PAGE_PROGRAM  0x02
#define CMD_SECTOR_ERASE  0x20
#define CMD_CHIP_ERASE    0xC7
#define CMD_JEDEC_ID      0x9F

static __xdata uint8_t  spi_cache[CACHE_SIZE];
static uint32_t          spi_cache_base  = 0xFFFFFFFFUL;
static uint8_t           spi_cache_valid = 0;

static uint8_t spi_xfer(uint8_t tx_data)
{
	uint8_t rx_data = 0;
	uint8_t i;

	for (i = 0; i < 8; i++)
	{
		if (tx_data & 0x80)
			SPI_MOSI = 1;
		else
			SPI_MOSI = 0;

		tx_data <<= 1;

		SPI_SCLK = 1;

		rx_data <<= 1;
		if (SPI_MISO)
			rx_data |= 1;

		SPI_SCLK = 0;
	}

	return rx_data;
}

static void spi_send_addr(uint32_t addr)
{
	spi_xfer((uint8_t)(addr >> 16));
	spi_xfer((uint8_t)(addr >> 8));
	spi_xfer((uint8_t)(addr));
}

static void spi_read_raw(uint32_t addr, uint8_t *buf, uint16_t len)
{
	SPI_CS_LOW();
	spi_xfer(CMD_READ);
	spi_send_addr(addr);
	while (len--)
	{
		*buf++ = spi_xfer(0xFF);
	}
	SPI_CS_HIGH();
}

static void cache_fill(uint32_t aligned_addr)
{
	spi_read_raw(aligned_addr, spi_cache, CACHE_SIZE);
	spi_cache_base  = aligned_addr;
	spi_cache_valid = 1;
}

static void spi_write_enable(void)
{
	SPI_CS_LOW();
	spi_xfer(CMD_WRITE_ENABLE);
	SPI_CS_HIGH();
}

static uint8_t spi_is_busy(void)
{
	uint8_t status;
	SPI_CS_LOW();
	spi_xfer(CMD_READ_STATUS);
	status = spi_xfer(0xFF);
	SPI_CS_HIGH();
	return (status & 0x01) ? 1 : 0;
}

static uint8_t spi_wait_busy(uint16_t timeout_ms)
{
	uint32_t start = GetSysMs();
	while (spi_is_busy())
	{
		if (GetSysMs() - start > (uint32_t)timeout_ms)
			return 1;
	}
	return 0;
}

void SpiFlash_Init(void)
{
	P3M1 &= ~(1 << 2); P3M0 |= (1 << 2);
	P3M1 &= ~(1 << 3); P3M0 |= (1 << 3);
	P3M1 &= ~(1 << 4); P3M0 &= ~(1 << 4);
	P3M1 &= ~(1 << 5); P3M0 |= (1 << 5);

	SPI_CS_HIGH();
	SPI_SCLK = 0;
	SPI_MOSI = 0;

	spi_cache_valid = 0;
}

uint8_t SpiFlash_ReadByte(uint32_t addr)
{
	uint8_t val;
	SPI_CS_LOW();
	spi_xfer(CMD_READ);
	spi_send_addr(addr);
	val = spi_xfer(0xFF);
	SPI_CS_HIGH();
	return val;
}

void SpiFlash_ReadBytes(uint32_t addr, uint8_t *buf, uint16_t len)
{
	spi_read_raw(addr, buf, len);
}

uint8_t SpiFlash_ReadCached(uint32_t addr)
{
	uint32_t aligned = addr & ~(uint32_t)CACHE_MASK;

	if (!spi_cache_valid || aligned != spi_cache_base)
		cache_fill(aligned);

	return spi_cache[addr & CACHE_MASK];
}

void SpiFlash_ReadBytesCached(uint32_t addr, uint8_t *buf, uint16_t len)
{
	while (len > 0)
	{
		uint32_t aligned   = addr & ~(uint32_t)CACHE_MASK;
		uint16_t offset    = (uint16_t)(addr & CACHE_MASK);
		uint16_t available = CACHE_SIZE - offset;
		uint16_t chunk     = (len < available) ? len : available;

		if (!spi_cache_valid || aligned != spi_cache_base)
			cache_fill(aligned);

		{
			uint8_t *src = spi_cache + offset;
			uint16_t i;
			for (i = 0; i < chunk; i++)
				*buf++ = *src++;
		}

		addr += chunk;
		len  -= chunk;
	}
}

void SpiFlash_CacheInvalidate(void)
{
	spi_cache_valid = 0;
}

uint8_t SpiFlash_SectorErase(uint32_t addr)
{
	spi_write_enable();
	SPI_CS_LOW();
	spi_xfer(CMD_SECTOR_ERASE);
	spi_send_addr(addr);
	SPI_CS_HIGH();
	spi_cache_valid = 0;
	return spi_wait_busy(1500);
}

uint8_t SpiFlash_ChipErase(void)
{
	spi_write_enable();
	SPI_CS_LOW();
	spi_xfer(CMD_CHIP_ERASE);
	SPI_CS_HIGH();
	spi_cache_valid = 0;
	return spi_wait_busy(30000);
}

uint8_t SpiFlash_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len)
{
	spi_write_enable();
	SPI_CS_LOW();
	spi_xfer(CMD_PAGE_PROGRAM);
	spi_send_addr(addr);
	while (len--)
		spi_xfer(*data++);
	SPI_CS_HIGH();
	spi_cache_valid = 0;
	return spi_wait_busy(1000);
}

void SpiFlash_ReadJedecId(uint8_t *id)
{
	SPI_CS_LOW();
	spi_xfer(CMD_JEDEC_ID);
	id[0] = spi_xfer(0xFF);
	id[1] = spi_xfer(0xFF);
	id[2] = spi_xfer(0xFF);
	SPI_CS_HIGH();
}
