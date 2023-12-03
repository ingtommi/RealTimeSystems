#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/adcc.h"
#include "mcc_generated_files/tmr1.h"
#include "mcc_generated_files/pin_manager.h"

#include "stdio.h"
#include "I2C/i2c.h"
#include "LCD/lcd.h"

#define MEASUREMENT_SIZE 6
#define TIME_SIZE 8

#define CLKH 0
#define CLKM 0
#define PMON 3
#define ALAF 1	// default 0
#define ALAH 1	// default 0
#define ALAM 0
#define ALAS 0
#define ALAT 25 // default 20
#define ALAL 2
#define TALA 5
#define NREC 4

unsigned char temperature;
unsigned char luminosity;

char measure_buffer[MEASUREMENT_SIZE];
char time_buffer[TIME_SIZE];

// Time variables (including possible initialized values)
volatile uint8_t hours = CLKH, minutes = CLKM, seconds = 0;

// Counters
volatile uint8_t sampling_count = PMON-1;
uint8_t pwm_count = 0;

// Flags
volatile bool sample = false;
bool alarm = false;

// Functions
void TMR1_IRQ (void);
void alarm_handler (void);
unsigned char readTC74 (void);
void normal_mode (void);

void main (void)
{	 
	// Initialization
	SYSTEM_Initialize();
	OpenI2C();
	LCDinit();
	
	// Interrupt configuration
	INTERRUPT_GlobalInterruptEnable();
	INTERRUPT_PeripheralInterruptEnable();
	TMR1_SetInterruptHandler(TMR1_IRQ);
	
	//RA6_SetLow(); // debug

	while (1)
	{
		// Show clock
		LCDcmd(0x80); // first line, first column
		//sprintf(time_buffer, "%02d:%02d:%02d", hours, minutes, seconds);
    sprintf(time_buffer, "%02d:%02d:%02d", hours, minutes, seconds-1);
		LCDstr(time_buffer);
		
		// ---------- NORMAL MODE ----------
		normal_mode();
	}
}

// ---- CLOCK ----

// TMR1 interrupt handler
void TMR1_IRQ (void) 
{
	// Toggle led D5
	IO_RA7_Toggle();
	
	// Increment clock
	//if (seconds < 59)
  if (seconds < 60)
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
	// Increment counter for sampling
	sampling_count++;
	// Set flag and clear counter (allows to clear counter even when config mode)
	if (sampling_count == PMON) {
		sample = true;
		sampling_count = 0;
	}
}

// ---- NORMAL MODE ----

// Condition true every PMON s
void normal_mode (void)
{
	if (sample)
	{
		// Sensor readings
		temperature = readTC74();
		luminosity = ADCC_GetSingleConversion(0x0) >> 8;

		// Temperature output
		LCDcmd(0xc0);									                    // second line, first column
		sprintf(measure_buffer, "%02d C ", temperature);  // extra space written to clear buffer if we lose one digit
		while (LCDbusy());
		LCDstr(measure_buffer);

		// Luminosity output
		LCDpos(1,14);
		sprintf(measure_buffer, "L%d", luminosity);
		while (LCDbusy());
		LCDstr(measure_buffer);

		// Clear flag
		sample = false;
	}

	// Condition true if alarm mode is enabled
	if (ALAF) 
	{
		// Notify alarm mode
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
		
		// Alarm LED is on for TALA s
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
	// Keep alarm LED off
	else
		RA6_SetLow();
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
	}	while (!(value & 0x40));

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

// Alarm handler
void alarm_handler (void) 
{
	// Set flag
	alarm = true;
	// Turn on alarm LED
	RA6_SetHigh();
}