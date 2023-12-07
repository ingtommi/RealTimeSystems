#include "mcc_generated_files/mcc.h"
#include "mcc_generated_files/tmr1.h"
#include "mcc_generated_files/pin_manager.h"

#include <stdio.h>
//#include <math.h>
#include "../I2C/i2c.h"
#include "../LCD/lcd.h"

#define MEASUREMENT_SIZE 6
#define TIME_SIZE        8
#define RECORD_SIZE     16

#define CLKH 0
#define CLKM 0
#define PMON 3
#define ALAF 1	// default 0
#define ALAH 1	// default 0
#define ALAM 0
#define ALAS 0
#define ALAT 20
#define ALAL 2
#define TALA 5
#define NREC 4
/* parameters needed to use TMR0 with period =! 5s
#define PS    8192 // max period with 1:8192 PS is around 8.3 s
#define FOSC4 64000000
*/
typedef enum 
{ 
    NORMAL, 
    CONFIG 
} modetype;

typedef struct
{
  uint8_t row;
  uint8_t column;
} LCDposition;

// Threshold variables
uint8_t alah = ALAH, alam = ALAM, alas = ALAS, alat = ALAT, alal = ALAL; // TODO: initialise them from memory
bool alaf = ALAF;

// Time variables (including possible initialized values)
volatile uint8_t hours = CLKH, minutes = CLKM, seconds = 0;

// Counters
volatile uint8_t sec_count = PMON-1;

// Flags
volatile bool time_1s, time_sample = false;
bool alarm = false;
volatile bool s1_pressed, s2_pressed = false;

// Functions
static void TMR0_IRQ (void);
static void TMR1_IRQ (void);
static void buttonsIRQ (void);

void banner (void);

void normal_mode (void);
void alarm_handler (void);
void clear_alarm (void);
unsigned char readTC74 (void);
void show_records ();

void config_mode (LCDposition *pos);
void modify_field (LCDposition *pos);
void modify_field_clk (void);
uint8_t col = 1; // needed by modify_field_clk, works only if defined here

void main (void)
{	 
	// Initialization
	SYSTEM_Initialize(); // NOTE: TMR0 is disabled (T0CON0 = 0x00)
	OpenI2C();
	LCDinit();
	
	// Interrupt configuration
	INTERRUPT_GlobalInterruptEnable();
	INTERRUPT_PeripheralInterruptEnable();
  TMR0_SetInterruptHandler(TMR0_IRQ);     // timer 0, period is 5 s
	TMR1_SetInterruptHandler(TMR1_IRQ);     // timer 1, period is 1 s
  IOCBF4_SetInterruptHandler(buttonsIRQ); // button S1
	IOCCF5_SetInterruptHandler(buttonsIRQ); // button S2
    
  // Start in NORMAL mode
  modetype mode = NORMAL;
  
  // Start cursor from the beginning
  LCDposition pos = {0, 0};

	while (1)
	{	
		switch(mode)
		{
			case NORMAL:
				
				normal_mode();                               // read sensors, show values and manage alarm
				
				if (s1_pressed)                                  // check S1
				// Clear alarm and keep mode or change it
				{
					__delay_ms(100);                                 // debouncing
					s1_pressed = false;                              // clear button flag
					if (!alarm)                                      // check alarm flag
					{
						LCDpos(0,11);
            while (LCDbusy());
						LCDstr("CTL");                                   // show CTL
						mode = CONFIG;                                   // change mode
					}
					else                                             // otherwise
          {
						clear_alarm();
            //mode = NORMAL;
				  }
        }
        
        /*else if (s2_pressed)                             // check S2
				// Show records and keep mode
				{
					__delay_ms(50);                                  // debouncing
					s2_pressed = false;                              // clear button flag
					show_records();                                  // call function
					//mode = NORMAL;                                 // keep mode
				}*/
        break;
			
			case CONFIG:
			
			  config_mode(&pos);                           // show thresholds
        
        // Move cursor back to right position
				LCDpos(pos.row, pos.column);
        
        if (s1_pressed)                              // check s1
			  // Move cursor and keep/change mode          
			  { 
				  __delay_ms(100);                             // debouncing
					s1_pressed = false;                          // clear button flag
          if (pos.row == 0 && pos.column == 15)        // check cursor end (second row is not used)
					{
						LCDpos(0,11);                                // erase CTL
            while (LCDbusy());
						LCDstr("   ");  
						pos.row = 0;                                 // re-initialise position
						pos.column = 0;
            mode = NORMAL;                               // change mode
					}
          else                                         // otherwise
            pos.column++;                                // move cursor
            //mode = CONFIG;                             // keep mode
        }
			
        else if (s2_pressed)                         // check s1
			  // Modify field and keep mode
				{
				  __delay_ms(50);                              // debouncing
					s2_pressed = false;                          // clear button flag
					modify_field(&pos);                          // call function
					//mode = CONFIG;                             // keep mode
				}
			  break;
		}
	}
}

// ---- INTERRUPTS ----

// TMR0 interrupt handler
static void TMR0_IRQ (void)
{
  // Disable TMR0 until new alarm
  TMR0_StopTimer(); 
  // Turn off A led
  PWM6_LoadDutyValue(0);
}

// TMR1 interrupt handler
static void TMR1_IRQ (void) 
{
	// Toggle led D5
	C_Toggle();
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
  // Set flags
  time_1s = true;
	sec_count++;
	if (sec_count == PMON) {
    sec_count = 0;
		time_sample = true;
	}
}

// S1/S2 interrupt handler
static void buttonsIRQ (void)
{
	// Check which button was pressed
	if (IOCBFbits.IOCBF4)
    s1_pressed = true;
	else
    s2_pressed = true;
}

// ---- BANNER ----
void banner (void)
{
  char time_buffer[TIME_SIZE];
  
  // Show clock
  sprintf(time_buffer, "%02d:%02d:%02d", hours, minutes, seconds);
  LCDcmd(0x80);  
  while (LCDbusy());
  LCDstr(time_buffer);
  // Show alarm mode
  LCDpos(0,15);
  while (LCDbusy());
  if (alaf) 
    LCDstr("A");
  else
    LCDstr("a");
}

// ---- NORMAL MODE ----

// Condition true every PMON s
void normal_mode (void)
{
  unsigned char temperature;
  unsigned char luminosity;
  
  char measure_buffer[MEASUREMENT_SIZE];
  
  banner();
    
	if (time_sample)
	{    
    // Clear flag
		time_sample = false;
    
		// Sensor readings
		//temperature = readTC74();
    temperature = 20; //DEBUG: ignore sensor, probably broken
		luminosity = ADCC_GetSingleConversion(0x0) >> 8;

		// Temperature output
		LCDcmd(0xc0);									  
		sprintf(measure_buffer, "%02u C ", temperature);  // extra space written to clear buffer if we lose one digit
		while (LCDbusy());
		LCDstr(measure_buffer);

		// Luminosity output
		LCDpos(1,14);
		sprintf(measure_buffer, "L%u", luminosity);
		while (LCDbusy());
		LCDstr(measure_buffer);
	}
  
	// Condition true if alarm mode is enabled
	if (alaf) 
	{
		// Check clock alarm
		if (hours == alah && minutes == alam && seconds == alas)
		{
      LCDpos(0,11);
      while (LCDbusy());
			LCDstr("C");
			alarm_handler();
		}
		// Check temperature alarm
		if (temperature > alat)	 
		{
      LCDpos(0,12);
      while (LCDbusy());
			LCDstr("T");
			// Turn on led D3
			T_SetHigh();
			alarm_handler();
		}
		// Check luminosity alarm
		if (luminosity > alal)	 
		{	
      LCDpos(0,13);
      while (LCDbusy());
			LCDstr("L");
			// Turn on led D2
			L_SetHigh();
			alarm_handler();
		}
	}
  // Keep cursor in first position
  LCDcmd(0x80);
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
	// Set alarm flag
	alarm = true;
	// Turn on alarm LED with 50% duty-cycle
  PWM6_LoadDutyValue(511);
  /* Load TMR0 with a value corresponding to TALA s
  uint8_t periodVal = round((FOSC4 * TALA) / (256 * PS)); // 153 if TALA = 5 s
  TMR0_Reload(periodVal);
  */
  // Enable TMR0 to count for 5 s
  TMR0_StartTimer();
}

// Clear alarm
void clear_alarm (void)
{
  // Turn off alarm LEDs
	L_SetLow();
	T_SetLow(); 
  // Clear any CTL character
	LCDpos(0,11);
  while (LCDbusy());
	LCDstr("   ");
  // Clear alarm flag
  alarm = false;
}

void show_records (void)
{
  /*
  char record_buffer1[RECORD_SIZE];
  char record_buffer2[RECORD_SIZE];
  
  for(uint8_i i = 0, i < NREC/2, i++)
  {
   if (time_1s)
   { 
     // TODO: read values from memory (hh_rec, etc...) 

     // 1st record
     sprintf(record_buffer1, "%02d:%02d:%02d %02u C L%u", hh_rec1, mm_rec1, ss_rec1, t_rec1, l_rec1);
     LCDcmd(0x80);
     while (LCDbusy());
     LCDstr(record_buffer1);

     // 2nd record
     sprintf(record_buffer2, "%02d:%02d:%02d %02u C L%u", hh_rec2, mm_rec2, ss_rec2, t_rec2, l_rec2);
     LCDcmd(0xc0);
     while (LCDbusy());
     LCDstr(record_buffer2);
   }
  }
  */
}

// ---- CONFIG MODE ----

void config_mode (LCDposition *pos)
{
  char threshold_buffer1[TIME_SIZE];
	char threshold_buffer2[MEASUREMENT_SIZE];
	
	// Cursor on C -> show C alarm hour, minute and seconds
	if (pos->row == 0 && pos->column == 11)
  {
    sprintf(threshold_buffer1, "%02d:%02d:%02d", alah, alam, alas);  // show thresholds instead of clock
    LCDcmd(0x80);
    while (LCDbusy());
		LCDstr(threshold_buffer1);
  }
	// Cursor on T -> show T alarm threshold
	else if (pos->row == 0 && pos->column == 12)
	{
    banner();                                                        // show clock                                                    
		sprintf(threshold_buffer2, "%02u C ", alat);                    
		LCDcmd(0xc0);
    while (LCDbusy());
		LCDstr(threshold_buffer2);
	}
	// Cursor on L -> show L alarm threshold
	else if (pos->row == 0 && pos->column == 13)
	{
    banner();                                                        // show clock  
    LCDcmd(0xc0);
    while (LCDbusy());
    LCDstr("      ");                                                // clear T threshold output
		sprintf(threshold_buffer2, "L%u", alal);                         
		LCDpos(1,14);
    while (LCDbusy());
		LCDstr(threshold_buffer2);
	}
	else
	{
    banner();                                                        // show clock  
		LCDcmd(0xc0);          
    while (LCDbusy());
		LCDstr("                ");                                      // clear the row
	}
}

// Modify field pointed by cursor
void modify_field (LCDposition *pos)
{
  // Cursor on C -> modify clock thresholds
	if (pos->row == 0 && pos->column == 11)
     modify_field_clk();   // set position to column 1
	// Cursor on T -> modify T threshold
	else if (pos->row == 0 && pos->column == 12)
    alat = (alat + 1) % 51; // range is [0,50]
	// Cursor on L -> modify L threshold
	else if (pos->row == 0 && pos->column == 13)
    alal = (alal + 1) % 4;  // range is [0,3]
  // Cursor on A -> modify A flag
	else if (pos->row == 0 && pos->column == 15)
    alaf = !alaf; 
}

void modify_field_clk (void) 
{ 
  char buffer[TIME_SIZE];
  
  while (1) {
    
    // show updated values
    sprintf(buffer, "%02d:%02d:%02d", alah, alam, alas);
    LCDpos(0,0);                         // start from hh:mm:ss
    while (LCDbusy());                   //             ^
    LCDstr(buffer);
  
    LCDpos(0, col);                      // move cursor
    while (!s1_pressed && !s2_pressed);  // wait for input
    __delay_ms(100);                     // debouncing

    switch (col)                         // check position
    {
      case 1:                            // hours
        if (s2_pressed)                  // S2 pressed
        {
          s2_pressed = false;
          alah = (alah + 1) % 24;          // increment
        }
        else                             // otherwise (S1 pressed)
        {
          s1_pressed = false;
          col = 4;                         // move column
        }
        break;
        
      case 4:                            // minutes
        if (s2_pressed)                  // S2 pressed
        {
          s2_pressed = false;
          alam = (alam + 1) % 60;          // increment
        }
        else                             // otherwise (S1 pressed)
        {
          s1_pressed = false;
          col = 7;                         // move column
        }
        break;
       
      case 7:                            // seconds
        if (s2_pressed)                  // S2 pressed
        {
          s2_pressed = false;              
          alas = (alas + 1) % 60;          // increment
        }
        else                             // otherwise (S1 pressed)
        {
          s1_pressed = false;     
          col = 1;
          return;                          // exit
        }
    }
  }
}