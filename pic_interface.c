/**The MIT License (MIT)

Copyright (c) 2015 by Greg Cunningham, CuriousTech

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// PIC10F320 Interface for WiFi Waterbed heater safety control and PWM buzzer output
// Rev.0

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <xc.h>

// PIC10F320 Configuration Bit Settings
#pragma config FOSC = INTOSC    // Oscillator Selection bits (INTOSC oscillator: CLKIN function disabled)
#pragma config BOREN = OFF      // Brown-out Reset Enable (Brown-out Reset disabled)
#pragma config WDTE = SWDTEN    // Watchdog Timer Enable (WDT controlled by the SWDTEN bit in the WDTCON register)
#pragma config PWRTE = OFF      // Power-up Timer Enable bit (PWRT disabled)
#pragma config MCLRE = OFF      // MCLR Pin Function Select bit (MCLR pin function is digital input, MCLR internally tied to VDD)
#pragma config CP = OFF         // Code Protection bit (Program memory code protection is disabled)
#pragma config LVP = OFF        // Low-Voltage Programming Enable (High-voltage on MCLR/VPP must be used for programming)
#pragma config LPBOR = OFF      // Brown-out Reset Selection bits (BOR disabled)
#pragma config BORV = LO        // Brown-out Reset Voltage Selection (Brown-out Reset Voltage (Vbor), low trip point selected.)
#pragma config WRT = OFF        // Flash Memory Self-Write Protection (Write protection off)

#define PWM_OUT     LATAbits.LATA0
#define HEAT_OUT    LATAbits.LATA1	// Heater
#define HEAT_IN     PORTAbits.RA2
#define BUZZ        PORTAbits.RA3  // BUZZ from ESP

void alarm(void);

#define _XTAL_FREQ 31000
bool beep;
uint8_t timer;

void interrupt isr(void)
{
	if(IOCAFbits.IOCAF2)	// HEAT_IN
	{
		IOCAFbits.IOCAF2 = 0;
		if(HEAT_IN)
			HEAT_OUT = 0;	// low = on
		timer = 3;			// 3 second timeout
	}
	if(IOCAFbits.IOCAF3)
	{
		IOCAFbits.IOCAF3 = 0;
		if(BUZZ)
			beep = true;
	}
}

void main(void)
{
	CLKRCONbits.CLKROE = 0;
	OSCCONbits.IRCF = 0b000;	// 31KHz

//	initialize peripherals and port I/O
	LATA = 0b0010;	// clear GPIO output latches
	ANSELA = 0;
	TRISA = 0b1100; // IIOO
	IOCAN = 0b1100;	// ints on ins
	IOCAP = 0b1100;

	CLC1CON = 0;
	CWG1CON0 = 0;

	OPTION_REG = 0b11000000;

	INTCONbits.IOCIE = 1;
	INTCONbits.GIE = 1;
	IOCAF = 0;			// clear all pin interrupts

	WDTCON = 0b00010101; //1 sec
	timer = 0;

	while(1)
	{
		asm("sleep");	// sleep 1 sec
		asm("nop");
		if(timer)
		{
			if(--timer == 0)
			{
				HEAT_OUT = 1;	// high = off
			}
		}
		if(beep)
		{
			alarm();
		}
	}
}

void alarm()
{
	OSCCONbits.IRCF = 0b011;   // 500KHz
#undef _XTAL_FREQ
#define _XTAL_FREQ 500000

	PWM1CON = 0b11000000;   //Enable PWM Module, Module Output

	PIR1bits.TMR2IF = 0;	//Clear the timer 2 interrupt flag 
	T2CON = 0b00000100;		//Turn TMR2 on, fastest re/post scale
	PWM1DCL = 100;
	PWM1DCH = 80;
	PR2 = 114;
	__delay_ms(30);

	PWM1CON = 0;   //clear PWM
	T2CON = 0;		//turn off timer
	OSCCONbits.IRCF = 0b000;   // 31KHz
}
