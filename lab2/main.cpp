#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "LM75B.h"
#include "C12832.h"
#include "semphr.h"
#include "shared.h" // custom header for shared objects

// FUNCTIONS
extern void monitor(void);
bool isInvalid(Time *time);
bool isInInterval(Record *record, Time *start);
bool isInInterval(Record *record, Time *start, Time *end);
  
BusOut leds(LED1,LED2,LED3,LED4);             // LEDs
PwmOut r(p23), g(p24), b(p25), speaker(p26);  // RGB LED and buzzer
C12832 lcd(p5, p7, p6, p8, p11);              // LCD
LM75B sensor(p28, p27);                       // T sensor
AnalogIn pot1(p19);                           // Potentiometer (L sensor)
Serial pc(USBTX, USBRX);                      // Serial

// TASKS
TaskHandle_t xSensorTimer, xProcessingTimer;

// QUEUES
QueueHandle_t xSensorInputQueue, xSensorOutputQueue, xProcessingInputQueue, xProcessingOutputQueue;

// SEMAPHORES & MUTEXES
SemaphoreHandle_t xAlarmSemaphore;
SemaphoreHandle_t xClockMutex, xPrintingMutex, xAlarmMutex, xBufferMutex, xParamMutex;

// SHARED DATA
uint8_t hours = 0, minutes = 0, seconds = 0;
uint8_t pmon = 3, tala = 5, pproc = 0; 
uint8_t alah = 0, alam = 0, alas = 0;
uint8_t alat = 20, alal = 2;
bool alaf = 0;                    // alaf = 0 --> a, alaf = 1 --> A
uint8_t temp, lum;                // sensors' values
uint8_t tala_count = 0;           // counter for TaskAlarm
uint8_t nr = 0, wi = 0, ri = 0;   // ring-buffer parameters (nr = valid records, wi = write index, ri = read index)
uint8_t n_unread_indices = 0;     // difference between wi and ri
Record records[NR];               // ring-buffer

const Time invalid = {INVALID, INVALID, INVALID};

// TIMERS (suspended if pmon/pproc are 0)
void vTaskSensorTimer(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount(); // needed by vTaskDelayUntil()
  Sender sender = TIMER;
  uint8_t delay;
  
  for (;;) 
  {
    // CRITICAL SECTION
    xSemaphoreTake(xParamMutex, portMAX_DELAY);
    delay = pmon;
    xSemaphoreGive(xParamMutex);
    // END OF CRITICAL SECTION
    xQueueSend(xSensorInputQueue, (void*)&sender, portMAX_DELAY); // unblock TaskSensors
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000 * delay));  // delay pmon sec
  }
}

void vTaskProcessingTimer(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount(); // needed by vTaskDelayUntil()
  Interval interval = {invalid, invalid};
  Sender sender = TIMER;
  InputData input = {interval, sender};  
  uint8_t delay;
  
  for (;;) 
  {
    // CRITICAL SECTION
    xSemaphoreTake(xParamMutex, portMAX_DELAY);
    delay = pproc;
    xSemaphoreGive(xParamMutex);
    // END OF CRITICAL SECTION
    xQueueSend(xProcessingInputQueue, (void*)&input, portMAX_DELAY); // unblock TaskSensors
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000 * delay));    // delay pproc sec
  }
}

// BUZZER
void vTaskAlarm(void *pvParameters)
{  
  for (;;) 
  {
    // Block until semaphore is given
    xSemaphoreTake(xAlarmSemaphore, portMAX_DELAY);
    
    if (tala_count != 0)
    {
      speaker = 0.5;                   // turn on buzzer
      tala_count--;
      xSemaphoreGive(xAlarmSemaphore); // give the semaphore to allow further execution
      vTaskDelay(pdMS_TO_TICKS(1000)); // delay 1 sec (vTaskDelayUntil was not working)
    }
    else
      speaker = 0;                     // turn off buzzer
      // Task will block because no semaphore is given
  }
}

// CLOCK
void vTaskClock(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount(); // needed by vTaskDelayUntil()
  
  for (;;)
  {
    /* CRITICAL SECTION: 
    - hours, minutes, seconds may be under user modification from TaskConsole
    - TaskSensor and TaskConsole may want to use the display
    */
    xSemaphoreTake(xClockMutex, portMAX_DELAY);
    // Print clock and alarm mode
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    lcd.locate(4,2);   // clock
    lcd.printf("%02u:%02u:%02u", hours, minutes, seconds);
    lcd.locate(117,2); // alarm mode
    if (alaf)
      lcd.printf("A");
    else
      lcd.printf("a");
    xSemaphoreGive(xPrintingMutex);
    
    // CRITICAL SECTION
    xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
    // Handle alarm
    if (alaf)
    {
      if (alah != 0 || alam != 0 || alas != 0) // do nothing if clock threshold is 00:00:00
      {
        if (hours == alah && minutes == alam && seconds == alas)
        {
          // CRITICAL SECTION: TaskSensor and TaskConsole may want to use the display
          xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
          lcd.locate(77, 2);
          lcd.printf("C");
          xSemaphoreGive(xPrintingMutex);
          // Unblock TaskAlarm
          tala_count = tala;
          xSemaphoreGive(xAlarmSemaphore);
        }
      }
    }
    // END OF CRITICAL SECTION
    xSemaphoreGive(xAlarmMutex);
    // END CRITICAL SECTION
    xSemaphoreGive(xClockMutex);

    // 1 sec delay
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));

    // CRITICAL SECTION: after the delay, hours, minutes and seconds may be again under modification
    xSemaphoreTake(xClockMutex, portMAX_DELAY);
    // Update time (after delay to start counting at 0 and have no issues with alarm)
    if (seconds < 59) seconds++;
    else
    {
      seconds = 0;
      if (minutes < 59) minutes++;
      else
      {
        minutes = 0;
        hours++;
      }
    }
    xSemaphoreGive(xClockMutex);
    // END OF CRITICAL SECTION
  }
}

// SENSORS
void vTaskSensors(void *pvParameters)
{
  Sender sender;
  
  for (;;)
  {
    // Blocked until element is written in the queue
    xQueueReceive(xSensorInputQueue, &sender, portMAX_DELAY);
    
    // Read data
    temp = (uint8_t)sensor.temp();
    lum = pot1.read_u16() >> 14; // convert L to {0...3}
    
    // CRITICAL SECTION
    xSemaphoreTake(xBufferMutex, portMAX_DELAY);
    // Save record
    records[wi].hours = hours;
    records[wi].minutes = minutes;
    records[wi].seconds = seconds;
    records[wi].temperature = temp;
    records[wi].luminosity = lum;
    // Increment index
    if (nr < NR) nr++;
    wi = (wi + 1) % NR;
    n_unread_indices++;                              // increment difference between wi and ri
    if (n_unread_indices == NR)                      // if wi finishes the ring and goes over ri
    {
      ri = (ri + 1 + (n_unread_indices - NR)) % NR;  // increment ri
      n_unread_indices = NR - 1;                     // decrement difference
    }
    xSemaphoreGive(xBufferMutex);
    // END OF CRITICAL SECTION
    
    // CRITICAL SECTION
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    // Print sensors' values
    lcd.locate(4,20);    // temperature
    lcd.printf("%u C ", temp); 
    lcd.locate(107, 20); // luminosity
    lcd.printf("L %u", lum);
    xSemaphoreGive(xPrintingMutex);
    
    if (sender == CONSOLE)
    {
      // Send data to user
      Sensor values = {temp, lum};
      xQueueSend(xSensorOutputQueue, (void*)&values, portMAX_DELAY);
    }

    // CRITICAL SECTION
    xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
    // Handle alarm
    if (alaf)
    {
      if (temp >= alat)
      {
        // CRITICAL SECTION
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(87,2);
        lcd.printf("T");
        xSemaphoreGive(xPrintingMutex);
        // Unblock TaskAlarm
        tala_count = tala;
        xSemaphoreGive(xAlarmSemaphore);
      }
      
      if (lum >= alal)
      {
        // CRITICAL SECTION
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(97,2);
        lcd.printf("L");
        xSemaphoreGive(xPrintingMutex);
        // Unblock TaskAlarm
        tala_count = tala;
        xSemaphoreGive(xAlarmSemaphore);
      }
    }
    xSemaphoreGive(xAlarmMutex);
    // END OF CRITICAL SECTION
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{
  InputData input;
  uint8_t temp, lum;
  uint8_t count, new_temp, new_lum, sum_lum;
  int sum_temp; // 8 bits are not enough
  uint8_t maxT, minT, maxL, minL;
  float meanT, meanL;
  
  for (;;)
  {
    // Blocked until element is written in the queue
    xQueueReceive(xProcessingInputQueue, &input, portMAX_DELAY);
    // Initialize variables
    count = 0; sum_temp = 0; sum_lum = 0; maxT = 0; minT = 50; maxL = 0; minL = 4;
    
    switch (input.sender)
    {
      case TIMER:
        // CRITICAL SECTION
        xSemaphoreTake(xBufferMutex, portMAX_DELAY);
        // Read data from memory
        if (nr != NR && n_unread_indices == 0) {} // don't read if there are no elements
        else
        {
          temp = records[ri].temperature;
          lum = records[ri].luminosity;
          // Increment index
          ri = (ri + 1) % NR;       
          if (n_unread_indices != 0) // if ri was behind wi, reduce distance
            n_unread_indices--;
        }
        xSemaphoreGive(xBufferMutex);
        // END OF CRITICAL SECTION
        
        // mutex not used because only r,b,leds only used here
        // RGB
        r = 1 - temp / 50.0; // temp = 50 -> r = 1, b = 0 (g = 0 always)
        b = temp / 50.0;
        // LEDs
        switch(lum)
        {
          case 0:
            leds = 0x1; // 0001
            break;
          case 1:
            leds = 0x3; // 0011
            break;
          case 2:
            leds = 0x7; // 0111
            break;
          case 3:
            leds = 0xf; // 1111
            break;
        }
        break;
        
      case CONSOLE:
        Time time1 = input.interval.time1;
        Time time2 = input.interval.time2;
        
        // Process totality of records
        if (isInvalid(&time1) && isInvalid(&time2))
        {
          for (uint8_t i = 0; i < nr; i++) // read valid records (not an issue if not ordered)
          {
            // CRITICAL SECTION
            xSemaphoreTake(xBufferMutex, portMAX_DELAY);
            new_temp = records[i].temperature;
            new_lum = records[i].luminosity;
            xSemaphoreGive(xBufferMutex);
            // END OF CRITICAL SECTION
            // T
            if (new_temp > maxT) maxT = new_temp; // max
            if (new_temp < minT) minT = new_temp; // min
            sum_temp += new_temp;
            // L
            if (new_lum > maxL) maxL = new_lum;   // max
            if (new_lum < minL) minL = new_lum;   // min
            sum_lum += new_lum;
          }
          meanT = sum_temp / float(nr);           // mean
          meanL = sum_lum / float(nr);
        }
        // Process from t1 to end
        else if (isInvalid(&time2))
        {
          for (uint8_t i = 0; i < nr; i++) // read valid records
          {
            // CRITICAL SECTION
            xSemaphoreTake(xBufferMutex, portMAX_DELAY);
            if (isInInterval(&records[i], &time1)) // save only records in interval
            {
              count++;                                   // count records in interval

              new_temp = records[i].temperature;
              new_lum = records[i].luminosity;
              // T
              if (new_temp > maxT) maxT = new_temp; // max
              if (new_temp < minT) minT = new_temp; // min
              sum_temp += new_temp;
              // L
              if (new_lum > maxL) maxL = new_lum;   // max
              if (new_lum < minL) minL = new_lum;   // min
              sum_lum += new_lum;
            }
            xSemaphoreGive(xBufferMutex);
            // END OF CRITICAL SECTION
          }
          meanT = sum_temp / float(count);          // mean
          meanL = sum_lum / float(count);
        }
        // Process from t1 to t2
        else
        {
          for (uint8_t i = 0; i < nr; i++) // read valid records
          {
            // CRITICAL SECTION
            xSemaphoreTake(xBufferMutex, portMAX_DELAY);
            if (isInInterval(&records[i], &time1, &time2))  // save only records in interval
            {
              count++;                              // count records in interval

              new_temp = records[i].temperature;
              new_lum = records[i].luminosity;
              // T
              if (new_temp > maxT) maxT = new_temp; // max
              if (new_temp < minT) minT = new_temp; // min
              sum_temp += new_temp;
              // L
              if (new_lum > maxL) maxL = new_lum;   // max
              if (new_lum < minL) minL = new_lum;   // min
              sum_lum += new_lum;
            }
            xSemaphoreGive(xBufferMutex);
            // END OF CRITICAL SECTION
          }
          meanT = sum_temp / float(count);          // mean
          meanL = sum_lum / float(count);
        }
        
        // Send data to user
        OutputData output = {maxT, minT, meanT, maxL, minL, meanL};
        xQueueSend(xProcessingOutputQueue, (void*)&output, portMAX_DELAY);
        break;
    }
  }
}

// CONSOLE
void vTaskConsole(void *pvParameters)
{
  for (;;) 
  {
    // Call console
    monitor();
  }
}

int main(void) {

  pc.baud(115200); // set baud rate

  // --- APPLICATION TASKS CAN BE CREATED HERE ---

  r = 1; g = 1; b = 1; // RGB off                   

  // Semaphores and mutexes
  xAlarmSemaphore = xSemaphoreCreateBinary();   // used to unblock Alarm  
  xClockMutex = xSemaphoreCreateMutex();        // used for hours, minutes, seconds
  xBufferMutex = xSemaphoreCreateMutex();       // used for 
  xPrintingMutex = xSemaphoreCreateMutex();     // used for lcd
  xAlarmMutex = xSemaphoreCreateMutex();        // used for alah, alam, alas, alat, alal, alaf
  xParamMutex = xSemaphoreCreateMutex();        // used for pmon, tala, pproc

  // Queues
  xSensorInputQueue = xQueueCreate(1, sizeof(Sender));
  xSensorOutputQueue = xQueueCreate(1, sizeof(Sensor));
  xProcessingInputQueue = xQueueCreate(1, sizeof(InputData));
  xProcessingOutputQueue = xQueueCreate(1, sizeof(OutputData));
  
  // Check if sufficient heap space
  if (xAlarmSemaphore == NULL || xClockMutex == NULL || xPrintingMutex == NULL || xBufferMutex == NULL || xAlarmMutex == NULL
      || xParamMutex == NULL || xSensorInputQueue == NULL || xSensorInputQueue == NULL || xProcessingInputQueue == NULL 
      || xProcessingOutputQueue == NULL)
  {
    printf("\nInsufficient heap space! Exiting...\n");
    return 1;
  }

  // Tasks
  xTaskCreate(vTaskAlarm, "Alarm", 2*configMINIMAL_STACK_SIZE, NULL, 4, NULL);
  xTaskCreate(vTaskSensorTimer, "TimerPMON", 2*configMINIMAL_STACK_SIZE, NULL, 4, &xSensorTimer);
  xTaskCreate(vTaskProcessingTimer, "TimerPPROC", 2*configMINIMAL_STACK_SIZE, NULL, 5, &xProcessingTimer);
  xTaskCreate(vTaskClock, "Clock", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskSensors, "Sensors", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskProcessing, "Processing", 2*configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  xTaskCreate(vTaskConsole, "Console", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  
  // Suspend TaskProcessingTimer because pproc is 0 by default. Task will be resumed when user modifies pproc.
  vTaskSuspend(xProcessingTimer);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for (;;);
  return 0;
}

// ---- UTILITY ----

bool isInvalid(Time *time)
{
  if (time->hours == invalid.hours) // && time->minutes == invalid.minutes && time->seconds == invalid.seconds)
    return true;
  return false;
}

bool isInInterval(Record *record, Time *start) 
{
  uint8_t r_hh = record->hours, r_mm = record->minutes, r_ss = record->seconds;                         // record
  uint8_t s_hh = uint8_t(start->hours), s_mm = uint8_t(start->minutes), s_ss = uint8_t(start->seconds); // start
  
  // record >= time1
  if (r_hh > s_hh ||                                
      r_hh == s_hh && r_mm > s_mm ||
      r_hh == s_hh && r_mm == s_mm && r_ss >= s_ss) 
    return true;
  return false;
}

bool isInInterval(Record *record, Time *start, Time *end)
{
  uint8_t r_hh = record->hours, r_mm = record->minutes, r_ss = record->seconds;                         // record
  uint8_t s_hh = uint8_t(start->hours), s_mm = uint8_t(start->minutes), s_ss = uint8_t(start->seconds); // start
  uint8_t e_hh = uint8_t(end->hours), e_mm = uint8_t(end->minutes), e_ss = uint8_t(end->seconds);       // end
  
  // record >= time1
  if (r_hh > s_hh ||                                
      r_hh == s_hh && r_mm > s_mm ||
      r_hh == s_hh && r_mm == s_mm && r_ss >= s_ss) 
  {
    // record <= time2
    if (r_hh < e_hh ||
        r_hh == e_hh && r_mm < e_mm ||
        r_hh == e_hh && r_mm == e_mm && r_ss <= e_ss)
      return true; 
  }
  return false;
}
