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

// Time variables (including possible initialized values)
volatile uint8_t hours = CLKH, minutes = CLKM, seconds = 0;

// Counters
volatile uint8_t sampling_count = PMON-1;
uint8_t pwm_count = 0;

// Flags
volatile bool sample = false;
bool alarm = false;
volatile bool s1_pressed, s2_pressed = false;

// Functions
void TMR1_IRQ (void);
void buttonsIRQ (void);

void normal_mode (void);
void config_mode (void);

void alarm_handler (void);
unsigned char readTC74 (void);
void clear_alarm (void);
void show_records (void);
void move_cursor (void);  // TODO: should take current position and return incremented position
void modify_field (void); // TODO: should take current position

void main (void)
{	 
  typedef enum 
	{ 
		NORMAL, 
		CONFIG 
	} mode;
	// Start in NORMAL mode
	mode = NORMAL;
	
	char time_buffer[TIME_SIZE];

	// Initialization
	SYSTEM_Initialize();
	OpenI2C();
	LCDinit();
	
	// Interrupt configuration
	INTERRUPT_GlobalInterruptEnable();
	INTERRUPT_PeripheralInterruptEnable();
	TMR1_SetInterruptHandler(TMR1_IRQ);     // timer 1
	IOCBF4_SetInterruptHandler(buttonsIRQ); // button S1
	IOCCF5_SetInterruptHandler(buttonsIRQ); // button S2
	
	while (1)
	{
		// Show clock
		LCDcmd(0x80);                                        // move to first line, first column
    sprintf(time_buffer, "%02d:%02d:%02d", hours, minutes, seconds);
		LCDstr(time_buffer);
		
		switch(mode)
		{
			case NORMAL:
				
				normal_mode();                                   // read sensors, show values and manage alarm
				
				if (s1_pressed)                                  // check S1
				// Clear alarm and keep mode or change it
				{
					__delay_ms(50);                                  // debouncing
					s1_pressed = false;                              // clear button flag
					if (alarm)                                       // check alarm flag
					{
						clear_alarm();                                   // call function
						//mode = NORMAL;                                 // keep mode
					}
					else
						RA6_SetLow();                                    // turn off A to avoid having it on indefinitely when in config mode
					  pwm_count = 0;                                   // clear counter used to count to TALA
						mode = CONFIG;                                   // change mode
				}                                                
				
				else if (s2_pressed)                             // check S2
				// Show records and keep mode
				{
					__delay_ms(50);                                  // debouncing
					s2_pressed = false;                              // clear button flag
					show_records();                                  // call function
					//mode = NORMAL;                                 // keep mode
				}
			  break;
			
			case CONFIG:
			
			  config_mode();                                   // show thresholds
			
				if (s1_pressed)                                  // check s1
			  // Move cursor and keep/change mode          
			  { 
				  __delay_ms(50);                                  // debouncing
					s1_pressed = false;                              // clear button flag
					//TODO: define it
				  next_position = move_cursor(current_position);   // call function [increment position and return it]
				  if (next_position == LCDpos(1,15))
					  mode = NORMAL;                                   // change mode [cursor end is LCDpos(1,15)]
				  //else mode = CONFIG;                              // keep mode
				  s1_pressed = false;                              // clear flag
			  }
				
				else if (s2_pressed)                             // check S2
				// Modify field and keep mode
				{
				  __delay_ms(50);                                  // debouncing
					s2_pressed = false;                              // clear button flag
					modify_field(current_position);                  // call function
					//mode = CONFIG;                                 // keep mode
				}
				break;
				
				//TODO: go to SLEEP()
	}
}

// ---- INTERRUPTS ----

// TMR1 interrupt handler
void TMR1_IRQ (void) 
{
	// Toggle led D5
	IO_RA7_Toggle();
	
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
	// Increment counter for sampling
	sampling_count++;
	// Set flag and clear counter (allows to clear counter even when config mode)
	if (sampling_count == PMON) {
		sample = true;
		sampling_count = 0;
	}
}

// S1/S2 interrupt handler
void buttonsIRQ(void)
{
	// Check which button was pressed
	if (S1_GetValue() == LOW)
		s1_pressed = true;
	else
		s2_pressed = true;
}

// ---- NORMAL MODE ----

void normal_mode (void)
{
	unsigned char temperature;
  unsigned char luminosity;
	char measure_buffer[MEASUREMENT_SIZE];
	
	// Condition true every PMON s
	if (sample)                                         
	{
		// Sensor readings
		temperature = readTC74();
		luminosity = ADCC_GetSingleConversion(0x0) >> 8;

		// Temperature output
		LCDcmd(0xc0);									                    // move to second line, first column
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
			IO_RA5_SetHigh(); // turn on T
			alarm_handler();
		}

		// Check luminosity alarm
		LCDpos(0,13);
		if (luminosity > ALAL)	 
		{	
			LCDstr("L");
			IO_RA4_SetHigh(); // turn on L
			alarm_handler();
		}
		
		// A is on for TALA s
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
}

// Read temperature sensor
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

// Manage alarm
void alarm_handler (void) 
{
	alarm = true;  // set alarm flag
	RA6_SetHigh(); // turn on A
}

// Clear alarm
void clear_alarm (void)
{
	alarm = false;   // clear alarm flag
	IO_RA4_SetLow(); // turn off L
	IO_RA5_SetLow(); // turn off T
	LCDcmd(0x80);    // move to first line, first column
	// Clear C character
	LCDpos(0,11);
	LCDstr(" ");
	// Clear T character
	LCDpos(0,12);
	LCDstr(" ");
	// Clear L character
	LCDpos(0,13);
	LCDstr(" ");
}

// TODO: define saving in memory and show_records()

// ---- CONFIG MODE ----

void config_mode (void)
{
	char threshold_buffer[MEASUREMENT_SIZE];
	
	// Temperature threshold output
	LCDcmd(0xc0);
	sprintf(threshold_buffer, "%02d C ", ALAT); // TODO: read parameter from memory if modified configuration
	while (LCDbusy());
	LCDstr(threshold_buffer);
	
	// Luminosity threshold output
	LCDpos(1,14);
	sprintf(measure_buffer, "L%d", ALAL);       // TODO: read parameter from memory if modified configuration
	while (LCDbusy());
	LCDstr(threshold_buffer);
}

// TODO: define move_cursor(current_position), modify_field(current_position)