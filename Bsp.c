#include "RegisterDefine.h"
#include "Player.h"
#include "Bsp.h"

#define MAIN_Fosc 22118400UL //定义主时钟
#define BaudRate1 115200UL   //选择波特率

#define Timer2_Reload (65536UL - (MAIN_Fosc / 4 / BaudRate1)) //Timer 2 重装值， 对应300KHZ

#define OUTPUT_PIN 5

#define MEASURE_S //PB_ODR |= (1 << OUTPUT_PIN)
#define MEASURE_E //PB_ODR &= ~(1 << OUTPUT_PIN)

#define CCP_S0 0x10 //P_SW1.4
#define CCP_S1 0x20 //P_SW1.5

extern __data Player mainPlayer;
extern __code unsigned char Score[];
void ADC_Inilize(void);
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
            //PlayerPlay(&mainPlayer, Score);
        }
        else if (r == 0xDD)
        {
            IAP_CONTR = 0x60;
        }
        else
        {
            NoteOnAsm(r);
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

    P1M1 &= ~(1 << 6), P1M0 |= (1 << 6); // P1.6 推挽输出
    P16 = 1;


    //P1M1 &= ~(1 << 7), P1M0 |= (1 << 7); // P1.7 推挽输出
    //P17 = 1;

    //P1M1 &= ~(1 << 2), P1M0 |= (1 << 2); // P1.2 推挽输出
    //P12 = 1;

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

    P_SW2 |= 1 << 7; // Enable XRAM reg access
    PWMA_PSCR = 0;

    PWMA_CCER1_Disable();             //关闭所有输入捕获/比较输出
    PWMA_CCER2_Disable();             //关闭所有输入捕获/比较输出
    
    PWMA_OC2ModeSet(CCMRn_PWM_MODE2); //设置输出比较模式
    PWMA_OC2_RelosdDisable();         //禁止输出比较的预装载
    PWMA_OC2_FastDisable();           //禁止输出比较快速功能
    PWMA_CC2E_Enable();               //开启输入捕获/比较输出


    PWMA_OC4ModeSet(CCMRn_PWM_MODE2); //设置输出比较模式
    PWMA_OC4_RelosdDisable();         //禁止输出比较的预装载
    PWMA_OC4_FastDisable();           //禁止输出比较快速功能
    PWMA_CC4NP_LowValid();
    PWMA_CC4NE_Enable();               //开启输入捕获/比较输出

    PWMA_ARRH = ( 256>> 8)&0xFF;
    PWMA_ARRL = 256&0xFF;

    PWMA_CCR2H = (256 >> 8)&0xFF;
    PWMA_CCR2L = 256&0xFF;

    PWMA_CCR4H = (0 >> 8)&0xFF;
    PWMA_CCR4L = 0&0xFF;

    PWMA_CCPCAPreloaded(0); //捕获/比较预装载控制位(该位只对具有互补输出的通道起作用)
    PWM2P_OUT_EN();
    PWM4N_OUT_EN();
    PWMA_DeadTime(0);         //死区发生器设置
    PWMA_BrakeOutputEnable(); //主输出使能
    PWMA_CEN_Enable();        //使能计数器

    PWM2_USE_P12P13();
    PWM4_USE_P16P17();

    // CCON = 0; //初始化PCA控制寄存器
    //           //PCA定时器停止
    //           //清除CF标志
    //           //清除模块中断标志
    // CL = 0;   //复位PCA寄存器
    // CH = 0;
    // CMOD = 0x08; //设置PCA时钟源
    //              //禁止PCA定时器溢出中断
    // // PCA_CLK_1T();
    // PCA_PWM0 = 0x00;        //PCA模块0工作于8位PWM
    // CCAP0H = CCAP0L = 0xFF; //PWM0的占空比为87.5% ((100H-20H)/100H)
    // CCAPM0 = 0x42;          //PCA模块0为8位PWM模式

    // PCA_PWM1 = 0x00;        //PCA模块1工作于8位PWM
    // CCAP1H = CCAP1L = 0xFF; //PWM1的占空比为75% ((80H-20H)/80H)
    // CCAPM1 = 0x42;          //PCA模块1为8位PWM模式

    // CR = 1; //PCA定时器开始工作
    ADC_Inilize();
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

//========================================================================
// 函数: void	ADC_Inilize(ADC_InitTypeDef *ADCx)
// 描述: ADC初始化程序.
// 参数: ADCx: 结构参数,请参考adc.h里的定义.
// 返回: none.
// 版本: V1.0, 2012-10-22
//========================================================================
void ADC_Inilize(void)
{
    uint8_t ADC_SMPduty = 31; //ADC 模拟信号采样时间控制, 0~31（注意： SMPDUTY 一定不能设置小于 10）
    uint8_t ADC_CsSetup = 0;  //ADC 通道选择时间控制 0(默认),1
    uint8_t ADC_CsHold = 1;   //ADC 通道选择保持时间控制 0,1(默认),2,3
    uint8_t ADC_Speed     = ADC_SPEED_2X1T;		//设置 ADC 工作时钟频率	ADC_SPEED_2X1T~ADC_SPEED_2X16T
    ADCCFG = (ADCCFG & ~ADC_SPEED_2X16T) |ADC_Speed;

    ADC_CONTR |= 0x80;
    ADCCFG |= (1 << 5); //AD转换结果右对齐。
    //ADCCFG &= ~(1<<5);	//AD转换结果左对齐。
    //EADC = 1;			//中断允许		ENABLE,DISABLE
    EADC = 0;
    ADC_Priority(Priority_0); //指定中断优先级(低到高) Priority_0,Priority_1,Priority_2,Priority_3

    P_SW2 |= 0x80;
    ADCTIM = (ADC_CsSetup << 7) | (ADC_CsHold << 5) | ADC_SMPduty; //设置 ADC 内部时序，ADC采样时间建议设最大值
    //P_SW2 &= 0x7f;
}

//========================================================================
// 函数: uint16_t	Get_ADCResult(uint8_t channel)
// 描述: 查询法读一次ADC结果.
// 参数: channel: 选择要转换的ADC.
// 返回: ADC结果.
// 版本: V1.0, 2012-10-22
//========================================================================
uint16_t Get_ADCResult(uint8_t channel) //channel = 0~15
{
    uint16_t adc;
    uint8_t i;

    if (channel > ADC_CH15)
        return 4096; //错误,返回4096,调用的程序判断
    ADC_RES = 0;
    ADC_RESL = 0;

    ADC_CONTR = (ADC_CONTR & 0xf0) | ADC_START | channel;
    //NOP(4); //对ADC_CONTR操作后要4T之后才能访问

    for (i = 0; i < 250; i++) //超时
    {
        if (ADC_CONTR & ADC_FLAG)
        {
            ADC_CONTR &= ~ADC_FLAG;
            if (ADCCFG & (1 << 5)) //转换结果右对齐。
            {
                adc = ((uint16_t)ADC_RES << 8) | ADC_RESL;
            }
            else //转换结果左对齐。
            {
#if ADC_RES_12BIT == 1
                adc = (uint16_t)ADC_RES;
                adc = (adc << 4) | ((ADC_RESL >> 4) & 0x0f);
#else
                adc = (uint16_t)ADC_RES;
                adc = (adc << 2) | ((ADC_RESL >> 6) & 0x03);
#endif
            }
            return adc;
        }
    }
    return 4096; //错误,返回4096,调用的程序判断
}