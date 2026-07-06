#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>

#define PROTO_SYNC        0x5A
#define PROTO_RSP_FLAG    0x80

#define CMD_PING          0x00
#define CMD_GET_INFO      0x01
#define CMD_RESET         0x02
#define CMD_UPTIME        0x03
#define CMD_MEM_INFO      0x04
#define CMD_AUDIO_INFO    0x05
#define CMD_ADC_READ      0x06
#define CMD_VOICE_DUMP    0x07
#define CMD_SYS_INFO      0x08
#define CMD_NOTE_ON       0x09
#define CMD_NOTE_OFF      0x0A

#define CMD_PLAY          0x10
#define CMD_STOP          0x11
#define CMD_PREV          0x12
#define CMD_NEXT          0x13
#define CMD_SET_SONG      0x14
#define CMD_GET_STATUS    0x15

#define CMD_FLASH_INFO    0x20
#define CMD_FLASH_ERASE   0x21
#define CMD_FLASH_ERASE_ALL 0x22
#define CMD_FLASH_READ    0x23
#define CMD_FLASH_WRITE   0x24
#define CMD_FLASH_READ_ID 0x25

#define STATUS_OK              0x00
#define STATUS_UNKNOWN_CMD     0x01
#define STATUS_BAD_CHECKSUM    0x02
#define STATUS_BAD_LEN         0x03
#define STATUS_FLASH_ERR       0x04
#define STATUS_NOT_SUPPORTED   0x05
#define STATUS_INVALID_ADDR    0x06

#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0

#define PSTATE_IDLE     0
#define PSTATE_CMD      1
#define PSTATE_LEN      2
#define PSTATE_DATA     3
#define PSTATE_CSUM     4

#define TX_IDLE         0
#define TX_SENDING      1

void Proto_Init(void);
void Proto_Process(void);
void Proto_ISR_Rx(uint8_t byte);
void Proto_ISR_TxNextByte(void);

#endif
