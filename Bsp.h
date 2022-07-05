#ifndef __BSP_H__
#define __BSP_H__

#include "RegisterDefine.h"

#define ADC_RES_12BIT	1		//0: MCU为10位ADC; 1: MCU为12位ADC


#define	ADC_P10		0x01	//IO引脚 Px.0
#define	ADC_P11		0x02	//IO引脚 Px.1
#define	ADC_P12		0x04	//IO引脚 Px.2
#define	ADC_P13		0x08	//IO引脚 Px.3
#define	ADC_P14		0x10	//IO引脚 Px.4
#define	ADC_P15		0x20	//IO引脚 Px.5
#define	ADC_P16		0x40	//IO引脚 Px.6
#define	ADC_P17		0x80	//IO引脚 Px.7
#define	ADC_P1_All	0xFF	//IO所有引脚

#define ADC_POWER	(1<<7)	//ADC 电源
#define ADC_START	(1<<6)	//ADC 转换启动控制位。自动清0
#define ADC_FLAG	(1<<5)	//ADC 转换结束标志位。软件清0
#define ADC_EPWMT	(1<<4)	//使能 PWM 同步触发 ADC 功能
#define ADC_CH0		0
#define ADC_CH1		1
#define ADC_CH2		2
#define ADC_CH3		3
#define ADC_CH4		4
#define ADC_CH5		5
#define ADC_CH6		6
#define ADC_CH7		7
#define ADC_CH8		8
#define ADC_CH9		9
#define ADC_CH10	10
#define ADC_CH11	11
#define ADC_CH12	12
#define ADC_CH13	13
#define ADC_CH14	14
#define ADC_CH15	15

#define ADC_SPEED_2X1T		0			//SYSclk/2/1
#define ADC_SPEED_2X2T		1			//SYSclk/2/2
#define ADC_SPEED_2X3T		2			//SYSclk/2/3
#define ADC_SPEED_2X4T		3			//SYSclk/2/4
#define ADC_SPEED_2X5T		4			//SYSclk/2/5
#define ADC_SPEED_2X6T		5			//SYSclk/2/6
#define ADC_SPEED_2X7T		6			//SYSclk/2/7
#define ADC_SPEED_2X8T		7			//SYSclk/2/8
#define ADC_SPEED_2X9T		8			//SYSclk/2/9
#define ADC_SPEED_2X10T		9			//SYSclk/2/10
#define ADC_SPEED_2X11T		10		//SYSclk/2/11
#define ADC_SPEED_2X12T		11		//SYSclk/2/12
#define ADC_SPEED_2X13T		12		//SYSclk/2/13
#define ADC_SPEED_2X14T		13		//SYSclk/2/14
#define ADC_SPEED_2X15T		14		//SYSclk/2/15
#define ADC_SPEED_2X16T		15		//SYSclk/2/16

#define ADC_LEFT_JUSTIFIED		0		//ADC Result left-justified
#define ADC_RIGHT_JUSTIFIED		1		//ADC Result right-justified


void HardwareInit(void);
void StartAudioOutput(void);
void StopAudioOutput(void);
uint8_t GetRandom(void);
void IntoPowerDown(void);

// These declaration must be included in main.c to generate ljmp in vector table.
extern void timer_isr() __interrupt(TIMER0_VECTOR) __using(1);
extern void UART1_int(void) __interrupt(UART1_VECTOR);

#endif