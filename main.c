#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "Player.h"
#include "STC15F_SDCC.h"

#define MAIN_Fosc 22118400L //定义主时钟
#define RX1_Lenth 32		//串口接收缓冲长度
#define BaudRate1 115200UL	//选择波特率

#define Timer1_Reload (65536UL - (MAIN_Fosc / 4 / BaudRate1)) //Timer 1 重装值， 对应300KHZ
#define Timer2_Reload (65536UL - (MAIN_Fosc / 4 / BaudRate1)) //Timer 2 重装值， 对应300KHZ

/*************	本地变量声明	**************/
u8 RX1_Buffer[RX1_Lenth]; //接收缓冲
u8 TX1_Cnt;				  //发送计数
u8 RX1_Cnt;				  //接收计数
__sbit B_TX1_Busy;		  //发送忙标志

#define OUTPUT_PIN 5

#define MEASURE_S //PB_ODR |= (1 << OUTPUT_PIN)
#define MEASURE_E //PB_ODR &= ~(1 << OUTPUT_PIN)

extern void TestProcess(void);
extern const unsigned char Score[];

Player mainPlayer;
// void timer_isr() __interrupt(TIM4_ISR)
// {
// 	TIM4_SR &= ~(1 << TIM4_SR_UIF);
// 	MEASURE_S;
// 	Player32kProc(&mainPlayer);
// 	MEASURE_E;
// }

// void uart1_isr() __interrupt(UART1_RXC_ISR)
// {
// 	NoteOnAsm(&(mainPlayer.mainSynthesizer),UART1_DR&0b00111111);
// }

extern void timer_isr() __interrupt(2) __using(1);

/********************* UART1中断函数************************/
void UART1_int(void) __interrupt(UART1_VECTOR)
{
	if (RI)
	{
		RI = 0;
		RX1_Buffer[RX1_Cnt] = SBUF; //保存一个字节
		if (++RX1_Cnt >= RX1_Lenth)
			RX1_Cnt = 0; //避免溢出处理
	}

	if (TI)
	{
		TI = 0;
		B_TX1_Busy = 0; //清除发送忙标志
	}
}

void HardwareInit(void)
{
	// CLK_CKDIVR = 0x00;
	// uart_init();
	// UART1_CR2 |= (1 << 5);
	// /* Set PD3 as output */
	// PB_DDR |= (1 << OUTPUT_PIN);
	// PB_CR1 |= (1 << OUTPUT_PIN);

	// PC_DDR |= (1 << 4);
	// PC_CR1 |= (1 << 4);

	// /* Prescaler = 4 */
	// TIM4_PSCR = 0b00000010;

	// /* Frequency = F_CLK / (2 * prescaler * (1 + ARR))
	//  *           = 16 MHz / (2 * 4 * (1 + 124)) = 32000 Hz */
	// TIM4_ARR = 124;
	// TIM4_IER |= (1 << TIM4_IER_UIE); // Enable Update Interrupt
	// TIM4_CR1 |= (1 << TIM4_CR1_CEN); // Enable TIM4

	// TIM2_CCMR3 |=   0X70;   //设置定时器2三通道(PD2)输出比较三模式
	// TIM2_CCMR3 |= 0X04;     //输出比较3预装载使能

	// TIM2_CCMR2 |=   0X70;   //设置定时器2二通道(PD3)输出比较三模式
	// TIM2_CCMR2 |= 0X04;     //输出比较3预装载使能

	// TIM2_CCER1 |= (0x03<<4);     //通道2使能，低电平有效，配置为输出
	// TIM2_CCER2 |= 0x03;     //通道3使能，低电平有效，配置为输出

	// //初始化时钟分频器为1，即计数器的时钟频率为Fmaster=8M/64=0.125MHZ
	// TIM2_PSCR = 0X00;
	// //初始化自动装载寄存器，决定PWM 方波的频率，Fpwm=0.125M/62500=2HZ
	// TIM2_ARRH = 0;
	// TIM2_ARRL = 0xFF;
	// //初始化比较寄存器，决定PWM 方波的占空比：5000/10000 = 50%
	// TIM2_CCR3H = 0;
	// TIM2_CCR3L = 122;
	// TIM2_CCR2H = 0;
	// TIM2_CCR2L = 123;

	// // 启动计数;更新中断失能

	// TIM2_IER = 0x00;
	// TIM2_CR1 |= 0x81;

	// PD_DDR |=(1<<2|1<<3);
	// PD_CR1 |=(1<<2|1<<3);
	// enable_interrupts();
}

void main()
{
	B_TX1_Busy = 0;
	RX1_Cnt = 0;
	TX1_Cnt = 0;

	S1_8bit();		 //8位数据
	S1_USE_P30P31(); //UART1 使用P30 P31口	默认
					 //	S1_USE_P36P37();		//UART1 使用P36 P37口
					 //	S1_USE_P16P17();		//UART1 使用P16 P17口

	/*
	TR1 = 0;			//波特率使用Timer1产生
	AUXR &= ~0x01;		//S1 BRT Use Timer1;
	AUXR |=  (1<<6);	//Timer1 set as 1T mode
	TH1 = (u8)(Timer1_Reload >> 8);
	TL1 = (u8)Timer1_Reload;
	TR1  = 1;
*/

	AUXR &= ~(1 << 4); //Timer stop		波特率使用Timer2产生
	AUXR |= 0x01;	   //S1 BRT Use Timer2;
	AUXR |= (1 << 2);  //Timer2 set as 1T mode
	TH2 = (u8)(Timer2_Reload >> 8);
	TL2 = (u8)Timer2_Reload;
	AUXR |= (1 << 4); //Timer run enable

	REN = 1; //允许接收
	ES = 1;	 //允许中断

	EA = 1; //允许全局中断

	while (1)
	{
		if (TX1_Cnt != RX1_Cnt) //收到过数据
		{
			if (!B_TX1_Busy) //发送空闲
			{
				B_TX1_Busy = 1;				//标志发送忙
				SBUF = RX1_Buffer[TX1_Cnt]; //发一个字节
				if (++TX1_Cnt >= RX1_Lenth)
					TX1_Cnt = 0; //避免溢出处理
			}
		}
	}

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
		printf("sdfsf%d", mainPlayer.status);
	}
}
