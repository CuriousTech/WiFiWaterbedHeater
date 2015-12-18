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

// PIC10F320 watchdog for WiFi Waterbed heater safety control and PWM buzzer output
// Rev.1

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

#define PWM_OUT 	LATAbits.LATA0
#define RESET_OUT	LATAbits.LATA1	// Heater
#define HEARTBEAT	PORTAbits.RA1  // Heartbeat from ESP
#define BEEP_IN 	PORTAbits.RA3

void alarm(void);

uint16_t timer;
bool beep;
bool reset;

void interrupt isr(void)
{
	if(IOCAFbits.IOCAF3)	// BEEP pin on ESP
	{
		IOCAFbits.IOCAF3 = 0;
		beep = true;
	}
	else if(IOCAFbits.IOCAF2)	// 1Hz Heartbeat
	{
		IOCAFbits.IOCAF2 = 0;
		if(BEEP_IN == 0 && beep == false)	// By setting BEEP low, this will begin to function
			timer = 10;			// 5 second timeout
	}
	else if(INTCONbits.TMR0IF)
	{
		INTCONbits.TMR0IF = 0;
		TMR0 = 13;
		if(timer && beep == false)	// Clock is much faster in PWM beep code
		{
			if(--timer == 0)
			{
				reset = true;
			}
		}
	}
}

void main(void)
{
	CLKRCONbits.CLKROE = 0;
	OSCCONbits.IRCF = 0b000;	// 31KHz
#define _XTAL_FREQ 31000

//	initialize peripherals and port I/O
	LATA = 0b0010;	// clear GPIO output latches (RESET = high)
	ANSELA = 0;
	TRISA = 0b1100; // IIOO
	IOCAN = 0b0100;	// low/high ints on inputs
	IOCAP = 0b1100;

	CLC1CON = 0;
	CWG1CON0 = 0;

	OPTION_REGbits.PS = 0b100;	// 1:32
	OPTION_REGbits.PSA = 0;		// Use prescaler
	OPTION_REGbits.T0CS = 0;	// internal
	OPTION_REGbits.INTEDG = 1;

	INTCONbits.TMR0IE = 1;
	INTCONbits.IOCIE = 1;	// Int on change
	INTCONbits.GIE = 1;	// Enable ints
	INTCONbits.TMR0IF = 0;
	IOCAF = 0;		// clear all pin interrupts
	TMR0 = 13;

	WDTCON = 0b00010100;	// disabled
	timer = 60*10;		// 10 minutes at powerup
	beep = false;
	reset = false;

	while(1)
	{
		if(reset)
		{
			reset = false;
			timer = 60*5;		// restart every 5 minutes if nothing happens
			RESET_OUT = 0;		// pulse the reset
			beep = true;
			alarm();
			beep = false;
			RESET_OUT = 1;
		}

		if(beep)	// beep from the ESP
		{
			alarm();
			beep = false;
		}
	}
}

void alarm()
{
#undef _XTAL_FREQ
#define _XTAL_FREQ 500000
	OSCCONbits.IRCF = 0b011; // 500KHz

	PWM1CON = 0b11000000;   //Enable PWM Module, Module Output
	PIR1bits.TMR2IF = 0;	//Clear the timer 2 interrupt flag 
	T2CON = 0b00000100;		//Turn TMR2 on, fastest pre/post scale
	PWM1DCL = 100;
	PWM1DCH = 80;
	PR2 = 114;
	__delay_ms(200);		// Just a short beep

	PWM1CON = 0;			//clear PWM
	T2CON = 0;				//turn off timer
	OSCCONbits.IRCF = 0b000; // 31KHz
}
