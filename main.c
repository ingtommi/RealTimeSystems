/**
  Generated Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This is the main file generated using PIC10 / PIC12 / PIC16 / PIC18 MCUs

  Description:
    This header file provides implementations for driver APIs for all modules selected in the GUI.
    Generation Information :
        Product Revision  :  PIC10 / PIC12 / PIC16 / PIC18 MCUs - 1.81.6
        Device            :  PIC16F18875
        Driver Version    :  2.00
*/

/*
    (c) 2018 Microchip Technology Inc. and its subsidiaries. 
    
    Subject to your compliance with these terms, you may use Microchip software and any 
    derivatives exclusively with Microchip products. It is your responsibility to comply with third party 
    license terms applicable to your use of third party software (including open source software) that 
    may accompany Microchip software.
    
    THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER 
    EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY 
    IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS 
    FOR A PARTICULAR PURPOSE.
    
    IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE, 
    INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND 
    WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP 
    HAS BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO 
    THE FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL 
    CLAIMS IN ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT 
    OF FEES, IF ANY, THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS 
    SOFTWARE.
*/

#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/adcc.h"
#include "mcc_generated_files/tmr1.h"
#include "mcc_generated_files/pin_manager.h"

#include "I2C/i2c.h"
#include "LCD/lcd.h"

#include "stdio.h"

#define MEASUREMENT_SIZE 6
#define TIME_SIZE 8

#define CLKH 0
#define CLKM 0
#define PMON 3
#define ALAF 1  //debug
#define ALAH 1  //debug
#define ALAM 0
#define ALAS 0
#define ALAT 25 //debug
#define ALAL 2
#define TALA 5
#define NREC 4

// Timer1 interrupt flag
volatile bool TMR1_overflow = false;

unsigned char temperature;
unsigned char luminosity;

char measure_buffer[MEASUREMENT_SIZE];
char time_buffer[TIME_SIZE];

// Time variables (including possible initialized values)
uint8_t hours = CLKH, minutes = CLKM, seconds = 0;

// Counter to assure sampling rate
uint8_t count = PMON-1;
uint8_t pwm_count = 0;

// Alarm variable
bool alarm = false;

// TMR1 interrupt handler
void TMR1_IRQ (void) {
    TMR1_overflow = true;
}

void alarm_handler (void) {
	alarm = true;
	// PWM
    RA6_SetHigh();
    
}

// Temperature sensor reading
unsigned char readTC74 (void)
{
	unsigned char value;
    do 
    {
		IdleI2C();
		StartI2C(); IdleI2C();
    
		WriteI2C(0x9a | 0x00); IdleI2C();
		WriteI2C(0x01); IdleI2C();
		RestartI2C(); IdleI2C();
		WriteI2C(0x9a | 0x01); IdleI2C();
		value = ReadI2C(); IdleI2C();
		NotAckI2C(); IdleI2C();
		StopI2C();
	} 	while (!(value & 0x40));

	IdleI2C();
	StartI2C(); IdleI2C();
	WriteI2C(0x9a | 0x00); IdleI2C();
	WriteI2C(0x00); IdleI2C();
	RestartI2C(); IdleI2C();
	WriteI2C(0x9a | 0x01); IdleI2C();
	value = ReadI2C(); IdleI2C();
	NotAckI2C(); IdleI2C();
	StopI2C();

	return value;
}

/*
 Main application
*/

void main (void)
{    
    // Device initialization
    SYSTEM_Initialize();
    
    INTERRUPT_GlobalInterruptEnable();
    INTERRUPT_PeripheralInterruptEnable();
	
    TMR1_SetInterruptHandler(TMR1_IRQ);

    // I2C initialization
    OpenI2C();
    //I2C_SCL = 1;
    //I2C_SDA = 1;
    //WPUC3 = 1;
    //WPUC4 = 1;
    
    RA6_SetLow();
    
	// LCD initialization
    LCDinit();

    while (1)
    {   
        // Condition true every 1 s
        if (TMR1_overflow)
		{	
            LCDcmd(0x80);
        
            sprintf(time_buffer, "%02d:%02d:%02d", hours, minutes, seconds);
            
            LCDstr(time_buffer);

            // Toggle led D5
            IO_RA7_Toggle();
  
            // Increment counter
            count++;
 
            // Condition true every PMON s
            if (count == PMON)
			{
                // Sensor readings
                temperature = readTC74();
                luminosity = ADCC_GetSingleConversion(0x0) >> 8;

                // Temperature output
                LCDcmd(0xc0);                                     // second line, first column
                sprintf(measure_buffer, "%02d C ", temperature);  // extra space written to clear buffer if we lose one digit
                while (LCDbusy());
                LCDstr(measure_buffer);

                // Luminosity output
                LCDpos(1,14);
                sprintf(measure_buffer, "L%d", luminosity);
                while (LCDbusy());
                LCDstr(measure_buffer);

                // Reset counter
                count = 0;
            }

            // Condition true if alarm is enabled
            if (ALAF) 
			{
                LCDpos(0,15);
                LCDstr("A");

                // Check clock alarm
                LCDpos(0,11);
                if (hours == ALAH && minutes == ALAM && seconds == ALAS)
			    {
                    LCDstr("C");
					alarm_handler();
			    }

                // Check temperature alarm
                LCDpos(0,12);
                if (temperature > ALAT)  
				{
                    LCDstr("T");
					// Turn on led D3
                    IO_RA5_SetHigh();
					alarm_handler();
		        }

                // Check luminosity alarm
                LCDpos(0,13);
                if (luminosity > ALAL)   
				{	
                    LCDstr("L");
					// Turn on led D2
                    IO_RA4_SetHigh();
					alarm_handler();
				}
                
                if (RA6_GetValue() == HIGH)
                {
                    pwm_count++;
                    if (pwm_count == TALA)
                    {
                        RA6_SetLow();
                        pwm_count = 0;
                    }
                }
            }
            else
            {
                RA6_SetLow();
            }

            // Increment clock
            if (seconds < 59)
                seconds++;
            else 
			{
                seconds = 0;
                if (minutes < 59)
                    minutes++;
                else 
				{
                    minutes=0;
                    hours++;
                }
            }
			
			// Clear interrupt flag
            TMR1_overflow = false;
        }
    }
}

/*
 End of File
*/