#ifndef __BSP_H__
#define __BSP_H__

#include "RegisterDefine.h"

void HardwareInit(void);
void StartAudioOutput(void);
void StopAudioOutput(void);

// These declaration must be included in main.c to generate ljmp in vector table.
extern void timer_isr() __interrupt(TIMER0_VECTOR) __using(1);
extern void UART1_int(void) __interrupt(UART1_VECTOR);

#endif