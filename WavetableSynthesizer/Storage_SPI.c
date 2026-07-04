//====================================================================
// Storage_SPI.c — 外部 SPI FLASH 后端 (ZD25WQ80B / W25Qxx 兼容)
//
// 编译条件: 定义 STORAGE_BACKEND_SPI 时启用
//
// 硬件连接 (STC8H3K64S2 TSSOP20 → ZD25WQ80B SOP-8):
//   P3.2  →  Flash Pin 6 (SCLK)    ✓
//   P3.3  →  Flash Pin 5 (SI)      ★ 用作 MOSI (输出)
//   P3.4  →  Flash Pin 2 (SO)      ★ 用作 MISO (输入)
//   P3.5  →  Flash Pin 1 (CS#)     ✓
//
// ★ 注意: PCB 上 P3.3(MISO_4) 和 P3.4(MOSI_4) 相对于
//    Flash 的 SI/SO 引脚是交叉连接的。若使用硬件 SPI
//    则 MOSI/MISO 角色对调，无法直接工作，故此处采用
//    GPIO 模拟 (bit-bang)，用 P3.3 作 MOSI、P3.4 作 MISO。
//    如需使用硬件 SPI 加速，将 P3.3 和 P3.4 飞线交换即可。
//
// SPI Mode 0: CPOL=0 (空闲低), CPHA=0 (前沿采样)
//
// 读缓存:
//   CACHE_SIZE 字节 XRAM 缓冲区，缓存一个对齐块的 SPI FLASH 数据。
//   流式顺序读取 (stream_read) 命中率 ≈ 1 - 1/CACHE_SIZE。
//   随机读取 (stream_u8/u16/u32) 受益于同一 header block 内多次访问。
//====================================================================

#include "Storage.h"

#ifdef STORAGE_BACKEND_SPI

#include "RegisterDefine.h"

/* ================================================================
 * Cache 配置
 *=============================================================== */

#define CACHE_SIZE  1024
#define CACHE_MASK   (CACHE_SIZE - 1)

/* ================================================================
 * GPIO 位定义 (使用原始 P3.x 命名，避免与 swapped SPI 标签混淆)
 *=============================================================== */

#define SPI_CS       P35
#define SPI_SCLK     P32
#define SPI_MOSI     P33   /* → Flash SI (Pin 5) */
#define SPI_MISO     P34   /* ← Flash SO (Pin 2) */

#define SPI_CS_LOW()   (SPI_CS = 0)
#define SPI_CS_HIGH()  (SPI_CS = 1)

#define SPI_FLASH_CMD_READ  0x03

/* ================================================================
 * 全局缓存
 *=============================================================== */

static __xdata uint8_t  spi_cache[CACHE_SIZE];
static uint32_t          spi_cache_base  = 0xFFFFFFFFUL;
static uint8_t           spi_cache_valid = 0;

/* ================================================================
 * 底层 GPIO 模拟 SPI (仅 cache_fill 时使用)
 *=============================================================== */

static uint8_t spi_bitbang_xfer(uint8_t tx_data)
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
    spi_bitbang_xfer((uint8_t)(addr >> 16));
    spi_bitbang_xfer((uint8_t)(addr >> 8));
    spi_bitbang_xfer((uint8_t)(addr));
}

static void spi_flash_read_bytes_uncached(uint32_t addr, uint8_t *buf, uint16_t len)
{
    SPI_CS_LOW();
    spi_bitbang_xfer(SPI_FLASH_CMD_READ);
    spi_send_addr(addr);
    while (len--)
    {
        *buf++ = spi_bitbang_xfer(0xFF);
    }
    SPI_CS_HIGH();
}

/* ================================================================
 * Cache 操作
 *=============================================================== */

static void cache_fill(uint32_t aligned_addr)
{
    spi_flash_read_bytes_uncached(aligned_addr, spi_cache, CACHE_SIZE);
    spi_cache_base  = aligned_addr;
    spi_cache_valid = 1;
}

static uint8_t spi_flash_read_cached(uint32_t addr)
{
    uint32_t aligned = addr & ~(uint32_t)CACHE_MASK;

    if (!spi_cache_valid || aligned != spi_cache_base)
    {
        cache_fill(aligned);
    }

    return spi_cache[addr & CACHE_MASK];
}

static void spi_flash_read_bytes_cached(uint32_t addr, uint8_t *buf, uint16_t len)
{
    while (len > 0)
    {
        uint32_t aligned    = addr & ~(uint32_t)CACHE_MASK;
        uint16_t offset     = (uint16_t)(addr & CACHE_MASK);
        uint16_t available  = CACHE_SIZE - offset;
        uint16_t chunk      = (len < available) ? len : available;

        if (!spi_cache_valid || aligned != spi_cache_base)
        {
            cache_fill(aligned);
        }

        {
            uint8_t *src = spi_cache + offset;
            uint16_t i;
            for (i = 0; i < chunk; i++)
            {
                *buf++ = *src++;
            }
        }

        addr += chunk;
        len  -= chunk;
    }
}

/* ================================================================
 * GPIO 初始化
 *=============================================================== */

void spi_storage_init(void)
{
    // P3.2 (SCLK): 推挽输出
    P3M1 &= ~(1 << 2); P3M0 |= (1 << 2);
    // P3.3 (用作 MOSI → Flash SI): 推挽输出
    P3M1 &= ~(1 << 3); P3M0 |= (1 << 3);
    // P3.4 (用作 MISO ← Flash SO): 准双向口 (输入)
    P3M1 &= ~(1 << 4); P3M0 &= ~(1 << 4);
    // P3.5 (CS#): 推挽输出
    P3M1 &= ~(1 << 5); P3M0 |= (1 << 5);

    SPI_CS_HIGH();
    SPI_SCLK = 0;
    SPI_MOSI = 0;

    spi_cache_valid = 0;
}

/* ================================================================
 * ScoreStream 接口实现
 *=============================================================== */

void stream_init(ScoreStream *s, uint32_t base, uint32_t size)
{
    s->base_addr = base;
    s->pos       = 0;
    s->size      = size;
}

void stream_sub(ScoreStream *s, ScoreStream *parent, uint32_t offset, uint32_t size)
{
    s->base_addr = parent->base_addr + offset;
    s->pos       = 0;
    s->size      = size;
}

uint8_t stream_read(ScoreStream *s, uint8_t *out)
{
    if (s->pos >= s->size)
        return 0;
    *out = spi_flash_read_cached(s->base_addr + s->pos);
    s->pos++;
    return 1;
}

uint8_t stream_peek(ScoreStream *s, uint8_t *out)
{
    if (s->pos >= s->size)
        return 0;
    *out = spi_flash_read_cached(s->base_addr + s->pos);
    return 1;
}

uint8_t stream_u8(ScoreStream *s, uint32_t offset)
{
    return spi_flash_read_cached(s->base_addr + offset);
}

uint16_t stream_u16(ScoreStream *s, uint32_t offset)
{
    uint8_t buf[2];
    spi_flash_read_bytes_cached(s->base_addr + offset, buf, 2);
    return (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
}

uint32_t stream_u32(ScoreStream *s, uint32_t offset)
{
    uint8_t buf[4];
    spi_flash_read_bytes_cached(s->base_addr + offset, buf, 4);
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}

#endif /* STORAGE_BACKEND_SPI */
