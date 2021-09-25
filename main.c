#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "STC15F_SDCC.h"

#define MAIN_Fosc 22118400L //定义主时钟
#define BaudRate1 460800UL	//选择波特率

#define Timer1_Reload (65536UL - (MAIN_Fosc / 4 / BaudRate1)) //Timer 1 重装值， 对应300KHZ
#define Timer2_Reload (65536UL - (MAIN_Fosc / 4 / BaudRate1)) //Timer 2 重装值， 对应300KHZ

#define OUTPUT_PIN 5

#define MEASURE_S //PB_ODR |= (1 << OUTPUT_PIN)
#define MEASURE_E //PB_ODR &= ~(1 << OUTPUT_PIN)

extern void TestProcess(void);
extern const unsigned char Score[];

extern __data Player mainPlayer;

extern void timer_isr() __interrupt(TIMER0_VECTOR) __using(1);

/********************* UART1中断函数************************/
void UART1_int(void) __interrupt(UART1_VECTOR)
{
	if (RI)
	{
		RI = 0;
		//RX1_Buffer[RX1_Cnt] = SBUF; //保存一个字节
	}

	if (TI)
	{
		//TI = 0;
	}
}

void HardwareInit(void)
{
	// if(GPIOx->Mode == GPIO_PullUp)		P1M1 &= ~GPIOx->Pin,	P1M0 &= ~GPIOx->Pin;	 //上拉准双向口
	// if(GPIOx->Mode == GPIO_HighZ)		P1M1 |=  GPIOx->Pin,	P1M0 &= ~GPIOx->Pin;	 //浮空输入
	// if(GPIOx->Mode == GPIO_OUT_OD)		P1M1 |=  GPIOx->Pin,	P1M0 |=  GPIOx->Pin;	 //开漏输出
	// if(GPIOx->Mode == GPIO_OUT_PP)		P1M1 &= ~GPIOx->Pin,	P1M0 |=  GPIOx->Pin;	 //推挽输出

	// P1M1 &= ~(1 << 2), P1M0 |= (1 << 2); // P1.2 推挽输出
	// P12 = 0;

	S1_8bit();
	S1_USE_P30P31(); //UART1 使用P30 P31口	默认
					 //	S1_USE_P36P37();		//UART1 使用P36 P37口
					 //	S1_USE_P16P17();		//UART1 使用P16 P17口

	/*
	TR1 = 0;			//波特率使用Timer1产生
	AUXR &= ~0x01;		//S1 BRT Use Timer1;
	AUXR |=  (1<<6);	//Timer1 set as 1T mode
	TH1 = (uint8_t)(Timer1_Reload >> 8);
	TL1 = (uint8_t)Timer1_Reload;
	TR1  = 1;
*/

	AUXR &= ~(1 << 4); //Timer stop		波特率使用Timer2产生
	AUXR |= 0x01;	   //S1 BRT Use Timer2;
	AUXR |= (1 << 2);  //Timer2 set as 1T mode
	TH2 = (uint8_t)(Timer2_Reload >> 8);
	TL2 = (uint8_t)Timer2_Reload;
	AUXR |= (1 << 4); //Timer run enable
	REN = 1;		  //允许接收

	//TR0 = 0;				   //停止计数
	//PT0 = 1;				   //高优先级中断
	//TMOD = (TMOD & ~0x03) | 0; //工作模式,0: 16位自动重装, 1: 16位定时/计数, 2: 8位自动重装, 3: 16位自动重装, 不可屏蔽中断
	// AUXR &= ~0x80;	//12T
	//AUXR |= 0x80;	   //1T
	//TMOD &= ~0x04;	   //定时
	//INT_CLKO &= ~0x01; //不输出时钟

	//TH0 = (uint8_t)((65536UL - 22118400 / 32000) >> 8);
	//TL0 = (uint8_t)(65536UL - 22118400 / 32000);

	//ES = 1; //允许串口中断

	//EA = 1; //允许全局中断
}

void main()
{
	PlayerInit(&mainPlayer, &synthForAsm);
	HardwareInit();
#ifndef RUN_TEST
	PlayerPlay(&mainPlayer, Score);
#else
	TestProcess();
#endif
	while (1)
	{
		PlayerProcess(&mainPlayer);
	}
	
}
