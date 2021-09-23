/*------------------------------------------------------------------*/

/* --- STC MCU International Limited -------------------------------*/

/* --- STC 1T Series MCU RC Demo -----------------------------------*/

/* --- Mobile: (86)13922805190 -------------------------------------*/

/* --- Fax: 86-0513-55012956,55012947,55012969 ---------------------*/

/* --- Tel: 86-0513-55012928,55012929,55012966 ---------------------*/

/* --- Web: www.GXWMCU.com -----------------------------------------*/

/* --- QQ:  800003751 ----------------------------------------------*/

/* If you want to use the program or the program referenced in the  */

/* article, please specify in which data and procedures from STC    */

/*------------------------------------------------------------------*/

#ifndef _STC15Fxxxx_H

#define _STC15Fxxxx_H

/*  BYTE Registers  */

__sfr __at(0x80) P0;
__sfr __at(0x81) SP;
__sfr __at(0x82) DPL;
__sfr __at(0x83) DPH;
__sfr __at(0x84) S4CON;
__sfr __at(0x85) S4BUF;
__sfr __at(0x87) PCON;
__sfr __at(0x88) TCON;
__sfr __at(0x89) TMOD;
__sfr __at(0x8A) TL0;
__sfr __at(0x8B) TL1;
__sfr __at(0x8C) TH0;
__sfr __at(0x8D) TH1;
__sfr __at(0x8E) AUXR;
__sfr __at(0x8F) WAKE_CLKO;
__sfr __at(0x8F) INT_CLKO;
__sfr __at(0x8F) AUXR2;
__sfr __at(0x8A) RL_TL0;
__sfr __at(0x8B) RL_TL1;
__sfr __at(0x8C) RL_TH0;
__sfr __at(0x8D) RL_TH1;
__sfr __at(0x90) P1;
__sfr __at(0x91) P1M1;
__sfr __at(0x92) P1M0;
__sfr __at(0x93) P0M1;
__sfr __at(0x94) P0M0;
__sfr __at(0x95) P2M1;
__sfr __at(0x96) P2M0;
__sfr __at(0x97) CLK_DIV;
__sfr __at(0x97) PCON2;
__sfr __at(0x98) SCON;
__sfr __at(0x99) SBUF;
__sfr __at(0x9A) S2CON;
__sfr __at(0x9B) S2BUF;
__sfr __at(0x9D) P1ASF;
__sfr __at(0xA0) P2;
__sfr __at(0xA1) BUS_SPEED;
__sfr __at(0xA2) AUXR1;
__sfr __at(0xA2) P_SW1;
__sfr __at(0xA8) IE;
__sfr __at(0xA9) SADDR;
__sfr __at(0xAA) WKTCL;
__sfr __at(0xAB) WKTCH;
__sfr __at(0xAC) S3CON;
__sfr __at(0xAD) S3BUF;
__sfr __at(0xAF) IE2;
__sfr __at(0xB0) P3;
__sfr __at(0xB1) P3M1;
__sfr __at(0xB2) P3M0;
__sfr __at(0xB3) P4M1;
__sfr __at(0xB4) P4M0;
__sfr __at(0xB5) IP2;
__sfr __at(0xB6) IPH2;
__sfr __at(0xB7) IPH;
__sfr __at(0xB8) IP;
__sfr __at(0xB9) SADEN;
__sfr __at(0xBA) P_SW2;
__sfr __at(0xBC) ADC_CONTR;
__sfr __at(0xBD) ADC_RES;
__sfr __at(0xBE) ADC_RESL;
__sfr __at(0xC0) P4;
__sfr __at(0xC1) WDT_CONTR;
__sfr __at(0xC2) IAP_DATA;
__sfr __at(0xC3) IAP_ADDRH;
__sfr __at(0xC4) IAP_ADDRL;
__sfr __at(0xC5) IAP_CMD;
__sfr __at(0xC6) IAP_TRIG;
__sfr __at(0xC7) IAP_CONTR;
__sfr __at(0xC2) ISP_DATA;
__sfr __at(0xC3) ISP_ADDRH;
__sfr __at(0xC4) ISP_ADDRL;
__sfr __at(0xC5) ISP_CMD;
__sfr __at(0xC6) ISP_TRIG;
__sfr __at(0xC7) ISP_CONTR;
__sfr __at(0xC8) P5;
__sfr __at(0xC9) P5M1;
__sfr __at(0xCA) P5M0;
__sfr __at(0xCB) P6M1;
__sfr __at(0xCC) P6M0;
__sfr __at(0xCD) SPSTAT;
__sfr __at(0xCE) SPCTL;
__sfr __at(0xCF) SPDAT;
__sfr __at(0xD0) PSW;
__sfr __at(0xD1) T4T3M;
__sfr __at(0xD2) T4H;
__sfr __at(0xD3) T4L;
__sfr __at(0xD4) T3H;
__sfr __at(0xD5) T3L;
__sfr __at(0xD6) T2H;
__sfr __at(0xD7) T2L;
__sfr __at(0xD2) TH4;
__sfr __at(0xD3) TL4;
__sfr __at(0xD4) TH3;
__sfr __at(0xD5) TL3;
__sfr __at(0xD6) TH2;
__sfr __at(0xD7) TL2;
__sfr __at(0xD2) RL_T4H;
__sfr __at(0xD3) RL_T4L;
__sfr __at(0xD4) RL_T3H;
__sfr __at(0xD5) RL_T3L;
__sfr __at(0xD6) RL_T2H;
__sfr __at(0xD7) RL_T2L;
__sfr __at(0xD8) CCON;
__sfr __at(0xD9) CMOD;
__sfr __at(0xDA) CCAPM0;
__sfr __at(0xDB) CCAPM1;
__sfr __at(0xDC) CCAPM2;
__sfr __at(0xE0) ACC;
__sfr __at(0xE1) P7M1;
__sfr __at(0xE2) P7M0;
__sfr __at(0xE6) CMPCR1;
__sfr __at(0xE7) CMPCR2;
__sfr __at(0xE8) P6;
__sfr __at(0xE9) CL;
__sfr __at(0xEA) CCAP0L;
__sfr __at(0xEB) CCAP1L;
__sfr __at(0xEC) CCAP2L;
__sfr __at(0xF0) B;
__sfr __at(0xF2) PCA_PWM0;
__sfr __at(0xF3) PCA_PWM1;
__sfr __at(0xF4) PCA_PWM2;
__sfr __at(0xF8) P7;
__sfr __at(0xF9) CH;
__sfr __at(0xFA) CCAP0H;
__sfr __at(0xFB) CCAP1H;
__sfr __at(0xFC) CCAP2H;
/*  BIT Registers  */

/*  PSW   */

__sbit __at(0xD7) CY;
__sbit __at(0xD6) AC;
__sbit __at(0xD5) F0;
__sbit __at(0xD4) RS1;
__sbit __at(0xD3) RS0;
__sbit __at(0xD2) OV;
__sbit __at(0xD1) F1;
__sbit __at(0xD0) P;
/*  TCON  */

__sbit __at(0x8F) TF1;
__sbit __at(0x8E) TR1;
__sbit __at(0x8D) TF0;
__sbit __at(0x8C) TR0;
__sbit __at(0x8B) IE1;
__sbit __at(0x8A) IT1;
__sbit __at(0x89) IE0;
__sbit __at(0x88) IT0;
/*  P0  */

__sbit __at(0x80) P00;
__sbit __at(0x81) P01;
__sbit __at(0x82) P02;
__sbit __at(0x83) P03;
__sbit __at(0x84) P04;
__sbit __at(0x85) P05;
__sbit __at(0x86) P06;
__sbit __at(0x87) P07;
/*  P1  */

__sbit __at(0x90) P10;
__sbit __at(0x91) P11;
__sbit __at(0x92) P12;
__sbit __at(0x93) P13;
__sbit __at(0x94) P14;
__sbit __at(0x95) P15;
__sbit __at(0x96) P16;
__sbit __at(0x97) P17;
__sbit __at(0x90) RXD2;
__sbit __at(0x91) TXD2;
__sbit __at(0x90) CCP1;
__sbit __at(0x91) CCP0;
__sbit __at(0x92) SPI_SS;
__sbit __at(0x93) SPI_MOSI;
__sbit __at(0x94) SPI_MISO;
__sbit __at(0x95) SPI_SCLK;
/*  P2  */

__sbit __at(0xA0) P20;
__sbit __at(0xA1) P21;
__sbit __at(0xA2) P22;
__sbit __at(0xA3) P23;
__sbit __at(0xA4) P24;
__sbit __at(0xA5) P25;
__sbit __at(0xA6) P26;
__sbit __at(0xA7) P27;
/*  P3  */

__sbit __at(0xB0) P30;
__sbit __at(0xB1) P31;
__sbit __at(0xB2) P32;
__sbit __at(0xB3) P33;
__sbit __at(0xB4) P34;
__sbit __at(0xB5) P35;
__sbit __at(0xB6) P36;
__sbit __at(0xB7) P37;
__sbit __at(0xB0) RXD;
__sbit __at(0xB1) TXD;
__sbit __at(0xB2) INT0;
__sbit __at(0xB3) INT1;
__sbit __at(0xB4) T0;
__sbit __at(0xB5) T1;
__sbit __at(0xB6) WR;
__sbit __at(0xB7) RD;
__sbit __at(0xB7) CCP2;
__sbit __at(0xB5) CLKOUT0;
__sbit __at(0xB4) CLKOUT1;
/*  P4  */

__sbit __at(0xC0) P40;
__sbit __at(0xC1) P41;
__sbit __at(0xC2) P42;
__sbit __at(0xC3) P43;
__sbit __at(0xC4) P44;
__sbit __at(0xC5) P45;
__sbit __at(0xC6) P46;
__sbit __at(0xC7) P47;
/*  P5  */

__sbit __at(0xC8) P50;
__sbit __at(0xC9) P51;
__sbit __at(0xCA) P52;
__sbit __at(0xCB) P53;
__sbit __at(0xCC) P54;
__sbit __at(0xCD) P55;
__sbit __at(0xCE) P56;
__sbit __at(0xCF) P57;
/*  SCON  */

__sbit __at(0x9F) SM0;
__sbit __at(0x9E) SM1;
__sbit __at(0x9D) SM2;
__sbit __at(0x9C) REN;
__sbit __at(0x9B) TB8;
__sbit __at(0x9A) RB8;
__sbit __at(0x99) TI;
__sbit __at(0x98) RI;
/*  IE   */

__sbit __at(0xAF) EA;
__sbit __at(0xAE) ELVD;
__sbit __at(0xAD) EADC;
__sbit __at(0xAC) ES;
__sbit __at(0xAB) ET1;
__sbit __at(0xAA) EX1;
__sbit __at(0xA9) ET0;
__sbit __at(0xA8) EX0;
/*  IP   */

/*

__sbit __at (0xBF) PPCA;
__sbit __at (0xBE) PLVD;
__sbit __at (0xBD) PADC;
__sbit __at (0xBC) PS;
__sbit __at (0xBB) PT1;
__sbit __at (0xBA) PX1;
__sbit __at (0xB9) PT0;
__sbit __at (0xB8) PX0;
*/

__sbit __at(0xE0) ACC0;
__sbit __at(0xE1) ACC1;
__sbit __at(0xE2) ACC2;
__sbit __at(0xE3) ACC3;
__sbit __at(0xE4) ACC4;
__sbit __at(0xE5) ACC5;
__sbit __at(0xE6) ACC6;
__sbit __at(0xE7) ACC7;
__sbit __at(0xF0) B0;
__sbit __at(0xF1) B1;
__sbit __at(0xF2) B2;
__sbit __at(0xF3) B3;
__sbit __at(0xF4) B4;
__sbit __at(0xF5) B5;
__sbit __at(0xF6) B6;
__sbit __at(0xF7) B7;
//							7     6     5    4    3    2    1     0    Reset Value

//sfr IE2       = 0xAF;		-     -     -    -    -    -   ESPI  ES2   0000,0000B	//Auxiliary Interrupt

#define SPI_INT_ENABLE() IE2 |= 2 //允许SPI中断

#define SPI_INT_DISABLE() IE2 &= ~2 //允许SPI中断

#define UART2_INT_ENABLE() IE2 |= 1 //允许串口2中断

#define UART2_INT_DISABLE() IE2 &= ~1 //允许串口2中断

//                                          7     6     5    4    3    2    1    0    Reset Value

//sfr IP      = 0xB8; //中断优先级低位      PPCA  PLVD  PADC  PS   PT1  PX1  PT0  PX0   0000,0000

//--------

__sbit __at(0xBF) PPCA;
__sbit __at(0xBE) PLVD;
__sbit __at(0xBD) PADC;
__sbit __at(0xBC) PS;
__sbit __at(0xBB) PT1;
__sbit __at(0xBA) PX1;
__sbit __at(0xB9) PT0;
__sbit __at(0xB8) PX0;
//                                           7      6      5     4     3     2    1     0        Reset Value

//sfr IPH   = 0xB7; //中断优先级高位       PPCAH  PLVDH  PADCH  PSH  PT1H  PX1H  PT0H  PX0H   0000,0000

//sfr IP2   = 0xB5; //                       -      -      -     -     -     -   PSPI   PS2   xxxx,xx00

//sfr IPH2  = 0xB6; //                       -      -      -     -     -     -   PSPIH  PS2H  xxxx,xx00

#define PPCAH 0x80

#define PLVDH 0x40

#define PADCH 0x20

#define PSH 0x10

#define PT1H 0x08

#define PX1H 0x04

#define PT0H 0x02

#define PX0H 0x01

#define PCA_InterruptFirst() PPCA = 1

#define LVD_InterruptFirst() PLVD = 1

#define ADC_InterruptFirst() PADC = 1

#define UART1_InterruptFirst() PS = 1

#define Timer1_InterruptFirst() PT1 = 1

#define INT1_InterruptFirst() PX1 = 1

#define Timer0_InterruptFirst() PT0 = 1

#define INT0_InterruptFirst() PX0 = 1

/*************************************************************************************************/

/*************************************************************************************************/

#define S1_DoubleRate() PCON |= 0x80

#define S1_SHIFT() SCON &= 0x3f

#define S1_8bit() SCON = (SCON & 0x3f) | 0x40

#define S1_9bit() SCON = (SCON & 0x3f) | 0xc0

#define S1_RX_Enable() SCON |= 0x10

#define S1_USE_P30P31() P_SW1 &= ~0xc0 //UART1 使用P30 P31口	默认

#define S1_USE_P36P37() P_SW1 = (P_SW1 & ~0xc0) | 0x40 //UART1 使用P36 P37口

#define S1_USE_P16P17() P_SW1 = (P_SW1 & ~0xc0) | 0x80 //UART1 使用P16 P17口

#define S1_TXD_RXD_SHORT() PCON2 |= (1 << 4) //将TXD与RXD连接中继输出

#define S1_TXD_RXD_OPEN() PCON2 &= ~(1 << 4) //将TXD与RXD连接中继断开	默认

#define S1_BRT_UseTimer2() AUXR |= 1

#define S1_BRT_UseTimer1() AUXR &= ~1

//						  7      6      5      4      3      2     1     0        Reset Value

//sfr S2CON = 0x9A;		S2SM0    -    S2SM2  S2REN  S2TB8  S2RB8  S2TI  S2RI      00000000B		 //S2 Control

#define S2_8bit() S2CON &= ~(1 << 7) //串口2模式0，8位UART，波特率 = 定时器2的溢出率 / 4

#define S2_9bit() S2CON |= (1 << 7) //串口2模式1，9位UART，波特率 = 定时器2的溢出率 / 4

#define S2_RX_Enable() S2CON |= (1 << 4) //允许串2接收

#define S2_MODE0() S2CON &= ~(1 << 7) //串口2模式0，8位UART，波特率 = 定时器2的溢出率 / 4

#define S2_MODE1() S2CON |= (1 << 7) //串口2模式1，9位UART，波特率 = 定时器2的溢出率 / 4

#define S2_RX_EN() S2CON |= (1 << 4) //允许串2接收

#define S2_RX_Disable() S2CON &= ~(1 << 4) //禁止串2接收

#define TI2 (S2CON & 2) != 0

#define RI2 (S2CON & 1) != 0

#define SET_TI2() S2CON |= 2

#define CLR_TI2() S2CON &= ~2

#define CLR_RI2() S2CON &= ~1

#define S2TB8_SET() S2CON |= 8

#define S2TB8_CLR() S2CON &= ~8

#define S2_Int_en() IE2 |= 1 //串口2允许中断

#define S2_USE_P10P11() P_SW2 &= ~1 //UART2 使用P1口	默认

#define S2_USE_P46P47() P_SW2 |= 1 //UART2 使用P4口

#define S3_USE_P00P01() P_SW2 &= ~2 //UART3 使用P0口	默认

#define S3_USE_P50P51() P_SW2 |= 2 //UART3 使用P5口

#define S4_USE_P02P03() P_SW2 &= ~4 //UART4 使用P0口	默认

#define S4_USE_P52P53() P_SW2 |= 4 //UART4 使用P5口

/**********************************************************/

#define Timer0_16bitAutoReload() TMOD &= ~0x03 //16位自动重装

#define Timer0_16bit() TMOD = (TMOD & ~0x03) | 0x01 //16位

#define Timer0_8bitAutoReload() TMOD = (TMOD & ~0x03) | 0x02 //8位自动重装

#define Timer0_16bitAutoRL_NoMask() TMOD |= 0x03 //16位自动重装不可屏蔽中断

#define Timer0_AsCounterP32() TMOD |= 4 //时器0用做计数器

#define Timer0_AsTimer() TMOD &= ~4 //时器0用做定时器

#define Timer0_ExtControlP34() TMOD |= 4 //时器0由外部INT0高电平允许定时计数

#define Timer0_Run() TR0 = 1 //允许定时器0计数

#define Timer0_Stop() TR0 = 0 //禁止定时器0计数

#define Timer0_InterruptEnable() ET0 = 1 //允许Timer1中断.

#define Timer0_InterruptDisable() ET0 = 0 //禁止Timer1中断.

#define Timer1_16bitAutoReload() TMOD &= ~0x30 //16位自动重装

#define Timer1_16bit() TMOD = (TMOD & ~0x30) | 0x10 //16位

#define Timer1_8bitAutoReload() TMOD = (TMOD & ~0x30) | 0x20 //8位自动重装

#define Timer1_16bitAutoRL_NoMask() TMOD |= 0x30 //16位自动重装不可屏蔽中断

#define Timer1_AsCounterP33() TMOD |= (1 << 6) //时器1用做计数器

#define Timer1_AsTimer() TMOD &= ~(1 << 6) //时器1用做定时器

#define Timer1_ExtControlP35() TMOD |= (1 << 7) //时器1由外部INT1高电平允许定时计数

#define Timer1_Run() TR1 = 1 //允许定时器1计数

#define Timer1_Stop() TR1 = 0 //禁止定时器1计数

#define Timer1_InterruptEnable() ET1 = 1 //允许Timer1中断.

#define Timer1_InterruptDisable() ET1 = 0 //禁止Timer1中断.

//						   7     6       5      4     3      2      1      0    Reset Value

//sfr AUXR  = 0x8E;		T0x12 T1x12 UART_M0x6  T2R  T2_C/T T2x12 EXTRAM  S1ST2  0000,0000	//Auxiliary Register

#define Timer0_1T() AUXR |= (1 << 7) //Timer0 clodk = fo

#define Timer0_12T() AUXR &= ~(1 << 7) //Timer0 clodk = fo/12	12分频,	default

#define Timer1_1T() AUXR |= (1 << 6) //Timer1 clodk = fo

#define Timer1_12T() AUXR &= ~(1 << 6) //Timer1 clodk = fo/12	12分频,	default

#define S1_M0x6() AUXR |= (1 << 5) //UART Mode0 Speed is 6x Standard

#define S1_M0x1() AUXR &= ~(1 << 5) //default,	UART Mode0 Speed is Standard

#define Timer2_Run() AUXR |= (1 << 4) //允许定时器2计数

#define Timer2_Stop() AUXR &= ~(1 << 4) //禁止定时器2计数

#define Timer2_AsCounterP31() AUXR |= (1 << 3) //时器2用做计数器

#define Timer2_AsTimer() AUXR &= ~(1 << 3) //时器2用做定时器

#define Timer2_1T() AUXR |= (1 << 2) //Timer0 clodk = fo

#define Timer2_12T() AUXR &= ~(1 << 2) //Timer0 clodk = fo/12	12分频,	default

#define Timer2_InterruptEnable() IE2 |= (1 << 2) //允许Timer2中断.

#define Timer2_InterruptDisable() IE2 &= ~(1 << 2) //禁止Timer2中断.

#define ExternalRAM_enable() AUXR |= 2 //允许外部XRAM，禁止使用内部1024RAM

#define InternalRAM_enable() AUXR &= ~2 //禁止外部XRAM，允许使用内部1024RAM

#define T0_pulseP34_enable() AUXR2 |= 1 //允许 T0 溢出脉冲在T0(P3.5)脚输出，Fck0 = 1/2 T0 溢出率，T0可以1T或12T。

#define T0_pulseP34_disable() AUXR2 &= ~1

#define T1_pulseP35_enable() AUXR2 |= 2 //允许 T1 溢出脉冲在T1(P3.4)脚输出，Fck1 = 1/2 T1 溢出率，T1可以1T或12T。

#define T1_pulseP35_disable() AUXR2 &= ~2

#define T2_pulseP30_enable() AUXR2 |= 4 //允许 T2 溢出脉冲在T1(P3.0)脚输出，Fck2 = 1/2 T2 溢出率，T2可以1T或12T。

#define T2_pulseP30_disable() AUXR2 &= ~4

#define T0_pulseP35(n) ET0 = 0, Timer0_AsTimer(), Timer0_1T(), Timer0_16bitAutoReload(), TH0 = (65536 - (n / 2 + MAIN_Fosc / 2) / (n)) / 256, TL0 = (65536 - (n / 2 + MAIN_Fosc / 2) / (n)) % 256, AUXR2 |= bit0, TR0 = 1 //fx=fosc/(2*M)/n,  M=1 or M=12

#define T1_pulseP34(n) ET1 = 0, Timer1_AsTimer(), Timer1_1T(), Timer1_16bitAutoReload(), TH1 = (65536 - (n / 2 + MAIN_Fosc / 2) / (n)) / 256, TL1 = (65536 - (n / 2 + MAIN_Fosc / 2) / (n)) % 256, AUXR2 |= bit1, TR1 = 1 //fx=fosc/(2*M)/n,  M=1 or M=12

#define T2_pulseP30(n) Timer2_InterruptDisable(), Timer2_AsTimer(), Timer2_1T(), TH2 = (65536 - (n / 2 + MAIN_Fosc / 2) / (n)) / 256, TL2 = (65536 - (n / 2 + MAIN_Fosc / 2) / (n)) % 256, AUXR2 |= bit2, Timer2_Run() //fx=fosc/(2*M)/n,  M=1 or M=12

#define Timer0_Load(n) TH0 = (n) / 256, TL0 = (n) % 256

#define Timer1_Load(n) TH1 = (n) / 256, TL1 = (n) % 256

#define Timer2_Load(n) TH2 = (n) / 256, TL2 = (n) % 256

#define Timer0_Load_us(n) TH0 = (65536 - MainFosc_KHZ * (n) / 1000) / 256, TL0 = (65536 - MainFosc_KHZ * (n) / 1000) % 256

#define Timer1_Load_us(n) TH1 = (65536 - MainFosc_KHZ * (n) / 1000) / 256, TL1 = (65536 - MainFosc_KHZ * (n) / 1000) % 256

#define Timer2_Load_us(n) TH2 = (65536 - MainFosc_KHZ * (n) / 1000) / 256, TL2 = (65536 - MainFosc_KHZ * (n) / 1000) % 256

//sfr WDT_CONTR = 0xC1; //Watch-Dog-Timer Control register

//                                      7     6     5      4       3      2   1   0     Reset Value

//                                  WDT_FLAG  -  EN_WDT CLR_WDT IDLE_WDT PS2 PS1 PS0    xx00,0000

#define D_WDT_FLAG (1 << 7)

#define D_EN_WDT (1 << 5)

#define D_CLR_WDT (1 << 4) //auto clear

#define D_IDLE_WDT (1 << 3) //WDT counter when Idle

#define D_WDT_SCALE_2 0

#define D_WDT_SCALE_4 1

#define D_WDT_SCALE_8 2 //T=393216*N/fo

#define D_WDT_SCALE_16 3

#define D_WDT_SCALE_32 4

#define D_WDT_SCALE_64 5

#define D_WDT_SCALE_128 6

#define D_WDT_SCALE_256 7

#define WDT_reset(n) WDT_CONTR = D_EN_WDT + D_CLR_WDT + D_IDLE_WDT + (n) //初始化WDT，喂狗

//						  7     6      5    4     3      2    1     0     Reset Value

//sfr PCON   = 0x87;	SMOD  SMOD0  LVDF  POF   GF1    GF0   PD   IDL    0001,0000	 //Power Control

//SMOD		//串口双倍速

//SMOD0

#define LVDF (1 << 5) //P4.6低压检测标志

//POF

//GF1

//GF0

//#define 	D_PD		2		//set 1, power down mode

//#define 	D_IDLE		1		//set 1, idle mode

#define MCU_IDLE() PCON |= 1 //MCU 进入 IDLE 模式

#define MCU_POWER_DOWN() PCON |= 2 //MCU 进入 睡眠 模式

//sfr ISP_CMD   = 0xC5;

#define ISP_STANDBY() ISP_CMD = 0 //ISP空闲命令（禁止）

#define ISP_READ() ISP_CMD = 1 //ISP读出命令

#define ISP_WRITE() ISP_CMD = 2 //ISP写入命令

#define ISP_ERASE() ISP_CMD = 3 //ISP擦除命令

//sfr ISP_TRIG  = 0xC6;

#define ISP_TRIG() ISP_TRIG = 0x5A, ISP_TRIG = 0xA5 //ISP触发命令

//							  7    6    5      4    3    2    1     0    Reset Value

//sfr IAP_CONTR = 0xC7;		IAPEN SWBS SWRST CFAIL  -   WT2  WT1   WT0   0000,x000	//IAP Control Register

#define ISP_EN (1 << 7)

#define ISP_SWBS (1 << 6)

#define ISP_SWRST (1 << 5)

#define ISP_CMD_FAIL (1 << 4)

#define ISP_WAIT_1MHZ 7

#define ISP_WAIT_2MHZ 6

#define ISP_WAIT_3MHZ 5

#define ISP_WAIT_6MHZ 4

#define ISP_WAIT_12MHZ 3

#define ISP_WAIT_20MHZ 2

#define ISP_WAIT_24MHZ 1

#define ISP_WAIT_30MHZ 0

#if (MAIN_Fosc >= 24000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_30MHZ

#elif (MAIN_Fosc >= 20000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_24MHZ

#elif (MAIN_Fosc >= 12000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_20MHZ

#elif (MAIN_Fosc >= 6000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_12MHZ

#elif (MAIN_Fosc >= 3000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_6MHZ

#elif (MAIN_Fosc >= 2000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_3MHZ

#elif (MAIN_Fosc >= 1000000L)

#define ISP_WAIT_FREQUENCY ISP_WAIT_2MHZ

#else

#define ISP_WAIT_FREQUENCY ISP_WAIT_1MHZ

#endif

/* ADC Register */

//								7       6      5       4         3      2    1    0   Reset Value

//sfr ADC_CONTR = 0xBC;		ADC_POWER SPEED1 SPEED0 ADC_FLAG ADC_START CHS2 CHS1 CHS0 0000,0000	//AD 转换控制寄存器

//sfr ADC_RES  = 0xBD;		ADCV.9 ADCV.8 ADCV.7 ADCV.6 ADCV.5 ADCV.4 ADCV.3 ADCV.2	  0000,0000	//A/D 转换结果高8位

//sfr ADC_RESL = 0xBE;												  ADCV.1 ADCV.0	  0000,0000	//A/D 转换结果低2位

//sfr ADC_CONTR  = 0xBC;	//直接用MOV操作，不要用与或

//sfr SPCTL  = 0xCE;	SPI控制寄存器

//   7       6       5       4       3       2       1       0    	Reset Value

//	SSIG	SPEN	DORD	MSTR	CPOL	CPHA	SPR1	SPR0		0x00

#define SPI_SSIG_None() SPCTL |= (1 << 7) //1: 忽略SS脚

#define SPI_SSIG_Enable() SPCTL &= ~(1 << 7) //0: SS脚用于决定主从机

#define SPI_Enable() SPCTL |= (1 << 6) //1: 允许SPI

#define SPI_Disable() SPCTL &= ~(1 << 6) //0: 禁止SPI

#define SPI_LSB_First() SPCTL |= (1 << 5) //1: LSB先发

#define SPI_MSB_First() SPCTL &= ~(1 << 5) //0: MSB先发

#define SPI_Master() SPCTL |= (1 << 4) //1: 设为主机

#define SPI_Slave() SPCTL &= ~(1 << 4) //0: 设为从机

#define SPI_SCLK_NormalH() SPCTL |= (1 << 3) //1: 空闲时SCLK为高电平

#define SPI_SCLK_NormalL() SPCTL &= ~(1 << 3) //0: 空闲时SCLK为低电平

#define SPI_PhaseH() SPCTL |= (1 << 2) //1:

#define SPI_PhaseL() SPCTL &= ~(1 << 2) //0:

#define SPI_Speed(n) SPCTL = (SPCTL & ~3) | (n) //设置速度, 0 -- fosc/4, 1 -- fosc/16, 2 -- fosc/64, 3 -- fosc/128

//sfr SPDAT  = 0xCF; //SPI Data Register                                                     0000,0000

//sfr SPSTAT  = 0xCD;	//SPI状态寄存器

//   7       6      5   4   3   2   1   0    	Reset Value

//	SPIF	WCOL	-	-	-	-	-	-

#define SPIF 0x80 //SPI传输完成标志。写入1清0。

#define WCOL 0x40 //SPI写冲突标志。写入1清0。

#define SPI_USE_P12P13P14P15() AUXR1 &= ~0x0c //将SPI切换到P12(SS) P13(MOSI) P14(MISO) P15(SCLK)(上电默认)。

#define SPI_USE_P24P23P22P21() AUXR1 = (AUXR1 & ~0x0c) | 0x04 //将SPI切换到P24(SS) P23(MOSI) P22(MISO) P21(SCLK)。

#define SPI_USE_P54P40P41P43() AUXR1 = (AUXR1 & ~0x0c) | 0x08 //将SPI切换到P54(SS) P40(MOSI) P41(MISO) P43(SCLK)。

/*

;PCA_PWMn:    7       6     5   4   3   2     1       0

;			EBSn_1	EBSn_0	-	-	-	-	EPCnH	EPCnL

;B5-B2:		保留

;B1(EPCnH):	在PWM模式下，与CCAPnH组成9位数。

;B0(EPCnL):	在PWM模式下，与CCAPnL组成9位数。

*/

#define PWM0_NORMAL() PCA_PWM0 &= ~3 //PWM0正常输出(默认)

#define PWM0_OUT_0() PCA_PWM0 |= 3 //PWM0一直输出0

#define PWM0_OUT_1() PCA_PWM0 &= ~3, CCAP0H = 0 //PWM0一直输出1

#define PWM1_NORMAL() PCA_PWM1 &= ~3 //PWM0正常输出(默认)

#define PWM1_OUT_0() PCA_PWM1 |= 3 //PWM0一直输出0

#define PWM1_OUT_1() PCA_PWM1 &= ~3, CCAP1H = 0 //PWM1一直输出1

#define PWM2_NORMAL() PCA_PWM2 &= ~3 //PWM1正常输出(默认)

#define PWM2_OUT_0() PCA_PWM2 |= 3 //PWM2一直输出0

#define PWM2_OUT_1() PCA_PWM2 &= ~3, CCAP2H = 0 //PWM2一直输出1

//						7     6     5     4     3     2     1     0     Reset Value

//sfr CCON   = 0xD8;	CF    CR    -     -     -    CCF2  CCF1  CCF0   00xx,xx00	//PCA 控制寄存器。

__sbit __at(0xD8) CCF0;
__sbit __at(0xD9) CCF1;
__sbit __at(0xDA) CCF2;
__sbit __at(0xDE) CR;
__sbit __at(0xDF) CF;
//					 7     6     5     4     3     2     1     0    Reset Value

//sfr CMOD  = 0xD9;	CIDL   -     -     -   CPS2   CPS1  CPS0  ECF   0xxx,0000	//PCA 工作模式寄存器。

#define PCA_IDLE_OFF() CMOD |= (1 << 7) //IDLE状态PCA停止计数。

#define PCA_IDLE_ON() CMOD &= ~(1 << 7) //IDLE状态PCA继续计数。

#define PCA_CLK_12T() CMOD &= ~0x0E //PCA计数脉冲选择外部晶振/12。	fosc/12

#define PCA_CLK_2T() CMOD = (CMOD & ~0x0E) + 2 //PCA计数脉冲选择外部晶振/2。	fosc/2

#define PCA_CLK_T0() CMOD = (CMOD & ~0x0E) + 4 //PCA计数脉冲选择Timer0中断，Timer0可通过AUXR寄存器设置成工作在12T或1T模式。

#define PCA_CLK_ECI() CMOD = (CMOD & ~0x0E) + 6 //PCA计数脉冲选择从ECI/P3.4脚输入的外部时钟，最大 fosc/2。

#define PCA_CLK_1T() CMOD = (CMOD & ~0x0E) + 8 //PCA计数脉冲选择外部晶振。		Fosc/1

#define PCA_CLK_4T() CMOD = (CMOD & ~0x0E) + 10 //PCA计数脉冲选择外部晶振/4。	Fosc/4

#define PCA_CLK_6T() CMOD = (CMOD & ~0x0E) + 12 //PCA计数脉冲选择外部晶振/6。	Fosc/6

#define PCA_CLK_8T() CMOD = (CMOD & ~0x0E) + 14 //PCA计数脉冲选择外部晶振/8。	Fosc/8

#define PCA_INT_ENABLE() CMOD |= 1 //PCA计数器溢出中断允许位，1---允许CF（CCON.7）产生中断。

#define PCA_INT_DISABLE() CMOD &= ~1 //PCA计数器溢出中断禁止。

//					    7      6       5        4       3       2       1      0    Reset Value

//sfr AUXR1 = 0xA2;	  S1_S1  S1_S0  CCP_S1   CCP_S0  SPI_S1   SPI_S0    -     DPS   0100,0000	//Auxiliary Register 1

#define PCA_USE_P12P11P10P37() AUXR1 &= ~0x30 //将PCA/PWM切换到P12(ECI) P11(CCP0) P10(CCP1) P37(CCP2)(上电默认)。

#define PCA_USE_P34P35P36P37() AUXR1 = (AUXR1 & ~0x30) | 0x10 //将PCA/PWM切换到P34(ECI) P35(CCP0) P36(CCP1) P37(CCP2)。

#define PCA_USE_P24P25P26P27() AUXR1 = (AUXR1 & ~0x30) | 0x20 //将PCA/PWM切换到P24(ECI) P25(CCP0) P26(CCP1) P27(CCP2)。

#define DPS_SEL1() AUXR1 |= 1 //1：选择DPTR1。

#define DPS_SEL0() AUXR1 &= ~1 //0：选择DPTR0(上电默认)。

/*									7     6      5      4     3     2     1     0     Reset Value

//sfr CCAPM0 = 0xDA;	PWM 寄存器  -   ECOM0  CAPP0  CAPN0  MAT0  TOG0  PWM0  ECCF0   x000,0000	//PCA 模块0 

//sfr CCAPM1 = 0xDB;	PWM 寄存器  -   ECOM1  CAPP1  CAPN1  MAT1  TOG1  PWM1  ECCF1   x000,0000	//PCA 模块1

//sfr CCAPM2 = 0xDC;	PWM 寄存器  -   ECOM2  CAPP2  CAPN2  MAT2  TOG2  PWM2  ECCF2   x000,0000	//PCA 模块2

;ECOMn = 1:	允许比较功能。

;CAPPn = 1:	允许上升沿触发捕捉功能。

;CAPNn = 1:	允许下降沿触发捕捉功能。

;MATn  = 1:	当匹配情况发生时，允许CCON中的CCFn置位。

;TOGn  = 1:	当匹配情况发生时，CEXn将翻转。(CEX0/PCA0/PWM0/P3.7,CEX1/PCA1/PWM1/P3.5)

;PWMn  = 1:	将CEXn设置为PWM输出。

;ECCFn = 1:	允许CCON中的CCFn触发中断。

;ECOMn CAPPn CAPNn MATn TOGn PWMn ECCFn

;  0     0     0    0    0    0     0		00H	未启用任何功能。

;  x     1     0    0    0    0     x	 	20H	16位CEXn上升沿触发捕捉功能。

;  x     0     1    0    0    0     x	 	10H	16位CEXn下降沿触发捕捉功能。

;  x     1     1    0    0    0     x	 	30H	16位CEXn/PCAn边沿（上、下沿）触发捕捉功能。

;  1     0     0    1    0    0     x	 	48H	16位软件定时器。

;  1     0     0    1    1    0     x	 	4CH	16位高速脉冲输出。

;  1     0     0    0    0    1     0		42H	8位PWM。无中断

;  1     1     0    0    0    1     1		63H	8位PWM。低变高可产生中断

;  1     0     1    0    0    1     1		53H	8位PWM。高变低可产生中断

;  1     1     1    0    0    1     1		73H	8位PWM。低变高或高变低均可产生中断

;*******************************************************************

;*******************************************************************/

#define PCA0_none() CCAPM0 = 0

#define PCA0_PWM(nbit) CCAPM0 = 0x42, PCA_PWM0 = (PCA_PWM0 & 0x0c) | ((8 - nbit) << 6)

#define PCA0_PWM_rise_int(nbit) CCAPM0 = 0x63, PCA_PWM0 = (PCA_PWM0 & 0x0c) | ((8 - nbit) << 6)

#define PCA0_PWM_fall_int(nbit) CCAPM0 = 0x53, PCA_PWM0 = (PCA_PWM0 & 0x0c) | ((8 - nbit) << 6)

#define PCA0_PWM_edge_int(nbit) CCAPM0 = 0x73, PCA_PWM0 = (PCA_PWM0 & 0x0c) | ((8 - nbit) << 6)

#define PCA0_capture_rise() CCAPM0 = (0x20 + 1)

#define PCA0_capture_fall() CCAPM0 = (0x10 + 1)

#define PCA0_capture_edge() CCAPM0 = (0x30 + 1)

#define PCA0_16bit_Timer() CCAPM0 = (0x48 + 1)

#define PCA0_High_Pulse() CCAPM0 = (0x4C + 1)

#define PCA1_none() CCAPM1 = 0

#define PCA1_PWM(nbit) CCAPM1 = 0x42, PCA_PWM1 = (PCA_PWM1 & 0x0c) | ((8 - nbit) << 6)

#define PCA1_PWM_rise_int(nbit) CCAPM1 = 0x63, PCA_PWM1 = (PCA_PWM1 & 0x0c) | ((8 - nbit) << 6)

#define PCA1_PWM_fall_int(nbit) CCAPM1 = 0x53, PCA_PWM1 = (PCA_PWM1 & 0x0c) | ((8 - nbit) << 6)

#define PCA1_PWM_edge_int(nbit) CCAPM1 = 0x73, PCA_PWM1 = (PCA_PWM1 & 0x0c) | ((8 - nbit) << 6)

#define PCA1_capture_rise() CCAPM1 = (0x20 + 1)

#define PCA1_capture_fall() CCAPM1 = (0x10 + 1)

#define PCA1_capture_edge() CCAPM1 = (0x30 + 1)

#define PCA1_16bit_Timer() CCAPM1 = (0x48 + 1)

#define PCA1_High_Pulse() CCAPM1 = (0x4C + 1)

#define PCA2_none() CCAPM2 = 0

#define PCA2_PWM(nbit) CCAPM2 = 0x42, PCA_PWM2 = (PCA_PWM2 & 0x0c) | ((8 - nbit) << 6)

#define PCA2_PWM_rise_int(nbit) CCAPM2 = 0x63, PCA_PWM2 = (PCA_PWM2 & 0x0c) | ((8 - nbit) << 6)

#define PCA2_PWM_fall_int(nbit) CCAPM2 = 0x53, PCA_PWM2 = (PCA_PWM2 & 0x0c) | ((8 - nbit) << 6)

#define PCA2_PWM_edge_int(nbit) CCAPM2 = 0x73, PCA_PWM2 = (PCA_PWM2 & 0x0c) | ((8 - nbit) << 6)

#define PCA2_capture_rise() CCAPM2 = (0x20 + 1)

#define PCA2_capture_fall() CCAPM2 = (0x10 + 1)

#define PCA2_capture_edge() CCAPM2 = (0x30 + 1)

#define PCA2_16bit_Timer() CCAPM2 = (0x48 + 1)

#define PCA2_High_Pulse() CCAPM2 = (0x4C + 1)

/* Above is STC additional SFR or change */

/**********************************************************/

typedef unsigned char u8;

typedef unsigned int u16;

typedef unsigned long u32;

/**********************************************************/


/**********************************************/

/****************************************************************/

//sfr INT_CLKO = 0x8F;	//附加的 SFR WAKE_CLKO (地址：0x8F)

/*

    7   6    5    4   3     2        1       0         Reset Value

    -  EX4  EX3  EX2  -   T2CLKO   T1CLKO  T0CLKO      0000,0000B

b6 -  EX4      : 外中断INT4允许

b5 -  EX3      : 外中断INT3允许

b4 -  EX2      : 外中断INT2允许

b2 - T1CLKO    : 允许 T2 溢出脉冲在P3.0脚输出，Fck1 = 1/2 T1 溢出率

b1 - T1CLKO    : 允许 T1 溢出脉冲在P3.4脚输出，Fck1 = 1/2 T1 溢出率

b0 - T0CLKO    : 允许 T0 溢出脉冲在P3.5脚输出，Fck0 = 1/2 T0 溢出率

*/

#define LVD_InterruptEnable() ELVD = 1

#define LVD_InterruptDisable() ELVD = 0

//sfr WKTCL = 0xAA;	//STC11F\10和STC15系列 唤醒定时器低字节

//sfr WKTCH = 0xAB;	//STC11F\10和STC15系列 唤醒定时器高字节

//	B7		B6	B5	B4	B3	B2	B1	B0		B7	B6	B5	B4	B3	B2	B1	B0

//	WKTEN				S11	S10	S9	S8		S7	S6	S5	S4	S3	S2	S1	S0	n * 560us

#define WakeTimerDisable() WKTCH &= 0x7f //WKTEN = 0		禁止睡眠唤醒定时器

#define WakeTimerSet(scale) WKTCL = (scale) % 256, WKTCH = (scale) / 256 | 0x80 //WKTEN = 1	允许睡眠唤醒定时器

//sfr CLK_DIV = 0x97; //Clock Divder 系统时钟分频  -     -      -       -     -  CLKS2 CLKS1 CLKS0 xxxx,x000

#define SYSTEM_CLK_1T() CLK_DIV &= ~0x07 //default

#define SYSTEM_CLK_2T() CLK_DIV = (CLK_DIV & ~0x07) | 1

#define SYSTEM_CLK_4T() CLK_DIV = (CLK_DIV & ~0x07) | 2

#define SYSTEM_CLK_8T() CLK_DIV = (CLK_DIV & ~0x07) | 3

#define SYSTEM_CLK_16T() CLK_DIV = (CLK_DIV & ~0x07) | 4

#define SYSTEM_CLK_32T() CLK_DIV = (CLK_DIV & ~0x07) | 5

#define SYSTEM_CLK_64T() CLK_DIV = (CLK_DIV & ~0x07) | 6

#define SYSTEM_CLK_128T() CLK_DIV = CLK_DIV | 7

#define MCLKO_P54_None() CLK_DIV &= ~0xc0 //主时钟不输出

#define MCLKO_P54_DIV1() CLK_DIV = (CLK_DIV & ~0xc0) | 0x40 //主时钟不分频输出

#define MCLKO_P54_DIV2() CLK_DIV = (CLK_DIV & ~0xc0) | 0x80 //主时钟2分频输出

#define MCLKO_P54_DIV4() CLK_DIV = CLK_DIV | 0xc0 //主时钟4分频输出

#define MCLKO_P34_None() CLK_DIV &= ~0xc0 //主时钟不输出

#define MCLKO_P34_DIV1() CLK_DIV = (CLK_DIV & ~0xc0) | 0x40 //主时钟不分频输出

#define MCLKO_P34_DIV2() CLK_DIV = (CLK_DIV & ~0xc0) | 0x80 //主时钟2分频输出

#define MCLKO_P34_DIV4() CLK_DIV = CLK_DIV | 0xc0 //主时钟4分频输出

//sfr BUS_SPEED = 0xA1; //Stretch register      -   -  -  -   -   -  EXRTS1  EXRTSS0 xxxx,xx10

#define BUS_SPEED_1T() BUS_SPEED = 0

#define BUS_SPEED_2T() BUS_SPEED = 1

#define BUS_SPEED_4T() BUS_SPEED = 2

#define BUS_SPEED_8T() BUS_SPEED = 3

/*   interrupt vector */

#define INT0_VECTOR 0

#define TIMER0_VECTOR 1

#define INT1_VECTOR 2

#define TIMER1_VECTOR 3

#define UART1_VECTOR 4

#define ADC_VECTOR 5

#define LVD_VECTOR 6

#define PCA_VECTOR 7

#define UART2_VECTOR 8

#define SPI_VECTOR 9

#define INT2_VECTOR 10

#define INT3_VECTOR 11

#define TIMER2_VECTOR 12

#define INT4_VECTOR 16

#define UART3_VECTOR 17

#define UART4_VECTOR 18

#define TIMER3_VECTOR 19

#define TIMER4_VECTOR 20

#define CMP_VECTOR 21

#define TRUE 1

#define FALSE 0

//=============================================================

//========================================

#define PolityLow 0 //低优先级中断

#define PolityHigh 1 //高优先级中断

//========================================

#define MCLKO_None 0

#define MCLKO_DIV1 1

#define MCLKO_DIV2 2

#define MCLKO_DIV4 3

#define ENABLE 1

#define DISABLE 0

#define STC15F_L2K08S2 8

#define STC15F_L2K16S2 16

#define STC15F_L2K24S2 24

#define STC15F_L2K32S2 32

#define STC15F_L2K40S2 40

#define STC15F_L2K48S2 48

#define STC15F_L2K56S2 56

#define STC15F_L2K60S2 60

#define IAP15F_L2K61S2 61

#endif
