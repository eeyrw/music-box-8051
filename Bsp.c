#include "RegisterDefine.h"
#include "Player.h"

#define MAIN_Fosc 22118400UL //定义主时钟
#define BaudRate1 460800UL   //选择波特率

#define Timer2_Reload (65536UL - (MAIN_Fosc / 4 / BaudRate1)) //Timer 2 重装值， 对应300KHZ

#define OUTPUT_PIN 5

#define MEASURE_S //PB_ODR |= (1 << OUTPUT_PIN)
#define MEASURE_E //PB_ODR &= ~(1 << OUTPUT_PIN)

#define CCP_S0 0x10 //P_SW1.4
#define CCP_S1 0x20 //P_SW1.5

extern __data Player mainPlayer;
extern __code unsigned char Score[];

/********************* UART1中断函数************************/
void UART1_int(void) __interrupt(UART1_VECTOR)
{
    if (RI)
    {
        RI = 0;
        //RX1_Buffer[RX1_Cnt] = SBUF; //保存一个字节
        //NoteOnAsm(SBUF);
        uint8_t r = SBUF;
        if (r == 0xFF)
        {
            PlayerPlay(&mainPlayer, Score);
        }
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


    P3M1 &= ~(1 << 2), P3M0 |= (1 << 2); // P3.2 推挽输出
    P3M1 &= ~(1 << 3), P3M0 |= (1 << 3); // P3.3 推挽输出

    P5M1 &= ~(1 << 5), P5M0 |= (1 << 5); // P5.5 推挽输出
    P55 = 0;

#ifdef STC15F

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
    AUXR |= 0x01;      //S1 BRT Use Timer2;
    AUXR |= (1 << 2);  //Timer2 set as 1T mode
    TH2 = (uint8_t)(Timer2_Reload >> 8);
    TL2 = (uint8_t)Timer2_Reload;
    AUXR |= (1 << 4); //Timer run enable
    REN = 1;          //允许接收

    ES = 1; //允许串口中断

#endif

#ifdef STC8

    // STC8GK08A series has only timer0 and timer1.
    SCON = 0x50;
    TMOD = 0x00;
    TL1 = Timer2_Reload;
    TH1 = Timer2_Reload >> 8;
    TR1 = 1;
    AUXR = 0x40;
    ES = 1; //允许串口中断

#endif

    TR0 = 0;                   //停止计数
    PT0 = 1;                   //高优先级中断
    TMOD = (TMOD & ~0x03) | 0; //工作模式,0: 16位自动重装, 1: 16位定时/计数, 2: 8位自动重装, 3: 16位自动重装, 不可屏蔽中断
    // AUXR &= ~0x80;	//12T
    AUXR |= 0x80;      //1T
    TMOD &= ~0x04;     //定时
    INT_CLKO &= ~0x01; //不输出时钟

    TH0 = (uint8_t)((65536UL - MAIN_Fosc / 32000) >> 8);
    TL0 = (uint8_t)(65536UL - MAIN_Fosc / 32000);

    CCON = 0; //初始化PCA控制寄存器
              //PCA定时器停止
              //清除CF标志
              //清除模块中断标志
    CL = 0;   //复位PCA寄存器
    CH = 0;
    CMOD = 0x08; //设置PCA时钟源
                 //禁止PCA定时器溢出中断
    // PCA_CLK_1T();
    PCA_PWM0 = 0x00;        //PCA模块0工作于8位PWM
    CCAP0H = CCAP0L = 0xFF; //PWM0的占空比为87.5% ((100H-20H)/100H)
    CCAPM0 = 0x42;          //PCA模块0为8位PWM模式

    PCA_PWM1 = 0x00;        //PCA模块1工作于8位PWM
    CCAP1H = CCAP1L = 0xFF; //PWM1的占空比为75% ((80H-20H)/80H)
    CCAPM1 = 0x42;          //PCA模块1为8位PWM模式

    CR = 1; //PCA定时器开始工作

    EA = 1; //允许全局中断
}

void StartAudioOutput(void)
{
    TR0 = 1;
    ET0 = 1;
}

void StopAudioOutput(void)
{
}