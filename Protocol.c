#include "Protocol.h"
#include "RegisterDefine.h"
#include "Player.h"
#include "Bsp.h"
#ifdef STORAGE_BACKEND_SPI
#include "SpiFlash.h"
#endif

#define RX_BUF_SIZE  256
#define RX_MASK      (RX_BUF_SIZE - 1)

#define PKT_BUF_SIZE 260
#define TX_BUF_SIZE  264

static __xdata uint8_t  rx_ring[RX_BUF_SIZE];
static volatile uint8_t  rx_wr;
static volatile uint8_t  rx_rd;

static __xdata uint8_t  pkt_data[PKT_BUF_SIZE];
static uint8_t           pkt_len;
static uint8_t           pkt_pos;
static uint8_t           pkt_state;
static uint8_t           pkt_csum;
static uint8_t           pkt_cmd;

static __xdata uint8_t  tx_buf[TX_BUF_SIZE];
static volatile uint8_t  tx_len;
static volatile uint8_t  tx_pos;
static volatile uint8_t  tx_state;


extern Player mainPlayer;

static uint8_t calc_csum(uint8_t start, uint8_t count)
{
	uint8_t i;
	uint8_t csum = 0;
	for (i = 0; i < count; i++)
		csum ^= tx_buf[start + i];
	return csum;
}

static void start_tx(void)
{
	tx_pos  = 1;
	tx_state = TX_SENDING;
	SBUF = tx_buf[0];
}

static void build_response_header(uint8_t cmd, uint8_t status, uint8_t len)
{
	tx_buf[0] = PROTO_SYNC;
	tx_buf[1] = cmd | PROTO_RSP_FLAG;
	tx_buf[2] = status;
	tx_buf[3] = len;
	tx_len    = 5 + len;
}

static void send_simple_response(uint8_t cmd, uint8_t status)
{
	build_response_header(cmd, status, 0);
	tx_buf[4] = calc_csum(1, 3);
	tx_len = 5;
	start_tx();
}

static void send_data_response(uint8_t cmd, uint8_t status,
                               const uint8_t *data, uint8_t len)
{
	uint8_t i;
	build_response_header(cmd, status, len);
	for (i = 0; i < len; i++)
		tx_buf[4 + i] = data[i];
	tx_buf[4 + len] = calc_csum(1, 3 + len);
	tx_len = 5 + len;
	start_tx();
}

static void send_response_ok(uint8_t cmd)
{
	send_simple_response(cmd, STATUS_OK);
}

static void send_response_err(uint8_t cmd, uint8_t err)
{
	send_simple_response(cmd, err);
}

static void proto_rx_put(uint8_t byte)
{
	uint8_t next = (rx_wr + 1) & RX_MASK;
	if (next != rx_rd)
	{
		rx_ring[rx_wr] = byte;
		rx_wr = next;
	}
}

static uint8_t proto_rx_available(void)
{
	return rx_wr != rx_rd;
}

static uint8_t proto_rx_get(void)
{
	uint8_t byte = rx_ring[rx_rd];
	rx_rd = (rx_rd + 1) & RX_MASK;
	return byte;
}

static void reset_parser(void)
{
	pkt_state = PSTATE_IDLE;
	pkt_pos   = 0;
	pkt_csum  = 0;
}

static void dispatch_command(void);

static void process_rx_byte(uint8_t byte)
{
	switch (pkt_state)
	{
	case PSTATE_IDLE:
		if (byte == PROTO_SYNC)
		{
			pkt_state = PSTATE_CMD;
			pkt_csum  = 0;
			pkt_pos   = 0;
		}
		return;

	case PSTATE_CMD:
		pkt_cmd  = byte;
		pkt_csum ^= byte;
		pkt_state = PSTATE_LEN;
		return;

	case PSTATE_LEN:
		pkt_len  = byte;
		pkt_csum ^= byte;
		if (pkt_len == 0)
			pkt_state = PSTATE_CSUM;
		else
		{
			pkt_state = PSTATE_DATA;
			pkt_pos   = 0;
		}
		return;

	case PSTATE_DATA:
		pkt_data[pkt_pos] = byte;
		pkt_csum ^= byte;
		pkt_pos++;
		if (pkt_pos >= pkt_len)
			pkt_state = PSTATE_CSUM;
		return;

	case PSTATE_CSUM:
		if (byte == pkt_csum)
			dispatch_command();
		reset_parser();
		return;
	}
}

static uint32_t read_u32_le(const uint8_t *p)
{
	return (uint32_t)p[0]
	     | ((uint32_t)p[1] << 8)
	     | ((uint32_t)p[2] << 16)
	     | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *p)
{
	return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

#ifdef STORAGE_BACKEND_SPI

static uint8_t flash_read_id_buf[3];

static void cmd_flash_info(void)
{
	uint8_t buf[9];
	SpiFlash_ReadJedecId(flash_read_id_buf);
	buf[0] = flash_read_id_buf[0];
	buf[1] = flash_read_id_buf[1];
	buf[2] = flash_read_id_buf[2];
	buf[3] = (SPI_FLASH_SIZE >> 24) & 0xFF;
	buf[4] = (SPI_FLASH_SIZE >> 16) & 0xFF;
	buf[5] = (SPI_FLASH_SIZE >> 8)  & 0xFF;
	buf[6] =  SPI_FLASH_SIZE        & 0xFF;
	buf[7] = (SPI_FLASH_SECTOR_SIZE >> 8) & 0xFF;
	buf[8] =  SPI_FLASH_SECTOR_SIZE       & 0xFF;
	send_data_response(CMD_FLASH_INFO, STATUS_OK, buf, 9);
}

static void cmd_flash_read_id(void)
{
	SpiFlash_ReadJedecId(flash_read_id_buf);
	send_data_response(CMD_FLASH_READ_ID, STATUS_OK, flash_read_id_buf, 3);
}

static void cmd_flash_read(void)
{
	uint16_t i;
	uint8_t *out;

	if (pkt_len < 6)
	{
		send_response_err(CMD_FLASH_READ, STATUS_BAD_LEN);
		return;
	}

	uint32_t addr = read_u32_le(pkt_data);
	uint16_t len  = read_u16_le(pkt_data + 4);

	if (len > 252)
	{
		send_response_err(CMD_FLASH_READ, STATUS_BAD_LEN);
		return;
	}

	build_response_header(CMD_FLASH_READ, STATUS_OK, len);
	out = tx_buf + 4;
	for (i = 0; i < len; i++)
		out[i] = SpiFlash_ReadByte(addr + (uint32_t)i);
	tx_buf[4 + len] = calc_csum(1, 3 + len);
	tx_len = 5 + len;

	SpiFlash_CacheInvalidate();
	start_tx();
}

static void cmd_flash_erase(void)
{
	if (pkt_len < 4)
	{
		send_response_err(CMD_FLASH_ERASE, STATUS_BAD_LEN);
		return;
	}
	if (SpiFlash_SectorErase(read_u32_le(pkt_data)))
		send_response_err(CMD_FLASH_ERASE, STATUS_FLASH_ERR);
	else
		send_response_ok(CMD_FLASH_ERASE);
}

static void cmd_flash_erase_all(void)
{
	if (SpiFlash_ChipErase())
		send_response_err(CMD_FLASH_ERASE_ALL, STATUS_FLASH_ERR);
	else
		send_response_ok(CMD_FLASH_ERASE_ALL);
}

static void cmd_flash_write(void)
{
	if (pkt_len < 5)
	{
		send_response_err(CMD_FLASH_WRITE, STATUS_BAD_LEN);
		return;
	}
	if (SpiFlash_PageProgram(read_u32_le(pkt_data), pkt_data + 4, pkt_len - 4))
		send_response_err(CMD_FLASH_WRITE, STATUS_FLASH_ERR);
	else
		send_response_ok(CMD_FLASH_WRITE);
}

#else

static void flash_not_supported(uint8_t cmd)
{
	send_response_err(cmd, STATUS_NOT_SUPPORTED);
}

#endif

static void dispatch_command(void)
{
	switch (pkt_cmd)
	{
	case CMD_PING:
	{
		const char *ver = "MusicBox-8051 v1.0";
		uint8_t len;
		for (len = 0; ver[len]; len++);
		send_data_response(CMD_PING, STATUS_OK, (const uint8_t *)ver, len);
		break;
	}

	case CMD_GET_INFO:
	{
		uint8_t buf[4];
		Player *p = &mainPlayer;
		buf[0] = FW_VERSION_MAJOR;
		buf[1] = FW_VERSION_MINOR;
#ifdef STORAGE_BACKEND_SPI
		buf[2] = 1;
#else
		buf[2] = 0;
#endif
		buf[3] = (uint8_t)(p->scheduler.maxScoreNum & 0xFF);
		send_data_response(CMD_GET_INFO, STATUS_OK, buf, 4);
		break;
	}

	case CMD_RESET:
		send_response_ok(CMD_RESET);
		while (tx_state != TX_IDLE);
		IAP_CONTR = 0x60;
		while (1);
		break;

	case CMD_UPTIME:
	{
		uint32_t ms = GetSysMs();
		uint8_t buf[4];
		buf[0] = (uint8_t)(ms);
		buf[1] = (uint8_t)(ms >> 8);
		buf[2] = (uint8_t)(ms >> 16);
		buf[3] = (uint8_t)(ms >> 24);
		send_data_response(CMD_UPTIME, STATUS_OK, buf, 4);
		break;
	}

	case CMD_MEM_INFO:
	{
		uint8_t buf[4];
		buf[0] = SP;
		buf[1] = 0xFF - SP;
		buf[2] = 0;
		buf[3] = 0;
		send_data_response(CMD_MEM_INFO, STATUS_OK, buf, 4);
		break;
	}

	case CMD_AUDIO_INFO:
	{
		uint8_t buf[4];
		uint8_t i, active = 0;
		for (i = 0; i < POLY_NUM; i++)
		{
			if (synthForAsm.SoundUnitUnionList[i].split.envelopeLevel > 0)
				active++;
		}
		buf[0] = (uint8_t)(synthForAsm.mixOut);
		buf[1] = (uint8_t)(synthForAsm.mixOut >> 8);
		buf[2] = synthForAsm.lastSoundUnit;
		buf[3] = active;
		send_data_response(CMD_AUDIO_INFO, STATUS_OK, buf, 4);
		break;
	}

	case CMD_ADC_READ:
	{
		if (pkt_len < 1)
		{
			send_response_err(CMD_ADC_READ, STATUS_BAD_LEN);
			break;
		}
		uint16_t val = Get_ADCResult(pkt_data[0]);
		uint8_t buf[2];
		buf[0] = (uint8_t)(val >> 8);
		buf[1] = (uint8_t)(val);
		send_data_response(CMD_ADC_READ, STATUS_OK, buf, 2);
		break;
	}

	case CMD_VOICE_DUMP:
	{
		uint8_t i, j, buf[80];
		for (i = 0; i < POLY_NUM; i++)
		{
			SoundUnitSplit *su = &synthForAsm.SoundUnitUnionList[i].split;
			j = i * 10;
			buf[j + 0] = su->increment_frac;
			buf[j + 1] = su->increment_int;
			buf[j + 2] = su->wavetablePos_frac;
			buf[j + 3] = (uint8_t)(su->wavetablePos_int);
			buf[j + 4] = (uint8_t)(su->wavetablePos_int >> 8);
			buf[j + 5] = su->envelopeLevel;
			buf[j + 6] = su->envelopePos;
			buf[j + 7] = (uint8_t)(su->val);
			buf[j + 8] = (uint8_t)(su->val >> 8);
			buf[j + 9] = (uint8_t)(su->sampleVal);
		}
		send_data_response(CMD_VOICE_DUMP, STATUS_OK, buf, 80);
		break;
	}

	case CMD_PLAY:
		PlayerPlay(&mainPlayer);
		send_response_ok(CMD_PLAY);
		break;

	case CMD_STOP:
		PlayerStop(&mainPlayer);
		send_response_ok(CMD_STOP);
		break;

	case CMD_PREV:
		PlaySchedulerPreviousScore(&mainPlayer);
		send_response_ok(CMD_PREV);
		break;

	case CMD_NEXT:
		PlaySchedulerNextScore(&mainPlayer);
		send_response_ok(CMD_NEXT);
		break;

	case CMD_SET_SONG:
		if (pkt_len < 1)
		{
			send_response_err(CMD_SET_SONG, STATUS_BAD_LEN);
			break;
		}
		SchedulerPlaySong(&mainPlayer, (int32_t)(uint8_t)pkt_data[0]);
		send_response_ok(CMD_SET_SONG);
		break;

	case CMD_GET_STATUS:
	{
		Player *p = &mainPlayer;
		uint8_t buf[3];
		buf[0] = (uint8_t)p->scheduler.currentScoreIndex;
		buf[1] = (uint8_t)p->scheduler.maxScoreNum;
		buf[2] = (p->decoder.status == STATUS_DECODING) ? 1 : 0;
		send_data_response(CMD_GET_STATUS, STATUS_OK, buf, 3);
		break;
	}

#ifdef STORAGE_BACKEND_SPI
	case CMD_FLASH_INFO:
		cmd_flash_info();
		break;
	case CMD_FLASH_READ_ID:
		cmd_flash_read_id();
		break;
	case CMD_FLASH_READ:
		cmd_flash_read();
		break;
	case CMD_FLASH_ERASE:
		cmd_flash_erase();
		break;
	case CMD_FLASH_ERASE_ALL:
		cmd_flash_erase_all();
		break;
	case CMD_FLASH_WRITE:
		cmd_flash_write();
		break;
#else
	case CMD_FLASH_INFO:
	case CMD_FLASH_READ_ID:
	case CMD_FLASH_READ:
	case CMD_FLASH_ERASE:
	case CMD_FLASH_ERASE_ALL:
	case CMD_FLASH_WRITE:
		flash_not_supported(pkt_cmd);
		break;
#endif

	default:
		send_response_err(pkt_cmd, STATUS_UNKNOWN_CMD);
		break;
	}
}

void Proto_ISR_Rx(uint8_t byte)
{
	proto_rx_put(byte);
}

void Proto_ISR_TxNextByte(void)
{
	if (tx_state == TX_SENDING)
	{
		if (tx_pos < tx_len)
			SBUF = tx_buf[tx_pos++];
		else
			tx_state = TX_IDLE;
	}
}

void Proto_Init(void)
{
	rx_wr  = 0;
	rx_rd  = 0;
	tx_len = 0;
	tx_pos = 0;
	tx_state = TX_IDLE;
	reset_parser();
}

void Proto_Process(void)
{
	while (proto_rx_available())
	{
		uint8_t byte = proto_rx_get();
		process_rx_byte(byte);
	}
}
