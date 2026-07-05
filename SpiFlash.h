#ifndef __SPI_FLASH_H__
#define __SPI_FLASH_H__

#include <stdint.h>

#define SPI_FLASH_SIZE        0x00100000UL
#define SPI_FLASH_SECTOR_SIZE 4096
#define SPI_FLASH_PAGE_SIZE   256
#define SPI_FLASH_BASE_ADDR   0x00000000UL

void SpiFlash_Init(void);

uint8_t SpiFlash_ReadByte(uint32_t addr);
void SpiFlash_ReadBytes(uint32_t addr, uint8_t *buf, uint16_t len);

uint8_t SpiFlash_ReadCached(uint32_t addr);
void SpiFlash_ReadBytesCached(uint32_t addr, uint8_t *buf, uint16_t len);
void SpiFlash_CacheInvalidate(void);

uint8_t SpiFlash_SectorErase(uint32_t addr);
uint8_t SpiFlash_ChipErase(void);
uint8_t SpiFlash_PageProgram(uint32_t addr, const uint8_t *data, uint16_t len);
void SpiFlash_ReadJedecId(uint8_t *id);

#endif
