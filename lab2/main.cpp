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

//DigitalOut led1(LED1);
C12832 lcd(p5, p7, p6, p8, p11);
LM75B sensor(p28, p27);
Serial pc(USBTX, USBRX);
AnalogIn pot1(p19); // TODO: fix potentiometer
PwmOut speaker(p26);

// TASKS
TaskHandle_t xSensorTimer, xProcessingTimer;

// QUEUES
QueueHandle_t xSensorInputQueue, xSensorOutputQueue, xProcessingInputQueue, xProcessingOutputQueue;

// SEMAPHORES & MUTEXES
SemaphoreHandle_t xAlarmSemaphore;
SemaphoreHandle_t xClockMutex, xPrintingMutex;

// SHARED DATA
uint8_t hours = 0, minutes = 0, seconds = 0;
uint8_t pmon = 3, tala = 5, pproc = 0; 
uint8_t alah = 0, alam = 0, alas = 0;
uint8_t alat = 20, alal = 2;
bool alaf = 0;                        // alaf = 0 --> a, alaf = 1 --> A
uint8_t temp, lum;                    // sensors' values
uint8_t tala_count = 0;               // counter for TaskAlarm
uint8_t nr = 0, wi = 0, ri = 0;       // ring-buffer parameters (nr = valid records, wi = write index, ri = read index)
uint8_t record_nr = 0;                // current index
Record records[NR];                   // ring-buffer
const Time invalid = {INVALID, INVALID, INVALID};

// TIMERS (suspended if pmon/pproc are 0)
void vTaskSensorTimer(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount(); // needed by vTaskDelayUntil()
  Sender sender = TIMER;
  
  for (;;) 
  {
    xQueueSend(xSensorInputQueue, &sender, portMAX_DELAY);       // unblock TaskSensors
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000 * pmon)); // delay pmon sec
  }
}

void vTaskProcessingTimer(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount(); // needed by vTaskDelayUntil()
  Interval interval = {invalid, invalid};
  Sender sender = TIMER;
  InputData input = {interval, sender};  
  
  for (;;) 
  {
    xQueueSend(xProcessingInputQueue, &input, portMAX_DELAY);      // unblock TaskSensors
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000 * pproc));  // delay pproc sec
  }
}

// BUZZER
void vTaskAlarm(void *pvParameters)
{
  TickType_t xLastWakeTime = xTaskGetTickCount(); // needed by vTaskDelayUntil()
  
  for (;;) 
  {
    // Block until semaphore is given
    xSemaphoreTake(xAlarmSemaphore, portMAX_DELAY);
    
    if (tala_count != 0)
    {
      speaker = 0.5;                                        // turn on buzzer
      tala_count--;
      xSemaphoreGive(xAlarmSemaphore);                      // give the semaphore to allow further execution
      vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000)); // delay 1 sec
    }
    else
      speaker = 0;                                          // turn off buzzer
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
    //CRITICAL SECTION: after the delay, hours, minutes and seconds may be again under modification
    xSemaphoreGive(xClockMutex);

    // 1 sec delay
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));

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
    temp = (uint8_t)sensor.temp(); // TODO: better to use a float?
    lum = (uint8_t)pot1.read();
    
    // Save record
    records[wi].hours = hours;
    records[wi].minutes = minutes;
    records[wi].seconds = seconds;
    records[wi].temperature = temp;
    records[wi].luminosity = lum;
    // Increment parameters
    wi++;
    wi %= NR;
    if (nr < NR) nr++;
    
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

    // Handle alarm
    if (alaf)
    {
      if (temp > alat)
      {
        // CRITICAL SECTION
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(87,2);
        lcd.printf("T", temp);
        xSemaphoreGive(xPrintingMutex);
        // Unblock TaskAlarm
        tala_count = tala;
        xSemaphoreGive(xAlarmSemaphore);
      }
      
      if (lum > alal)
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
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{
  InputData input;
  Time time1, time2;
  uint8_t temp, lum, maxT, minT, maxL, minL;
  float meanT, meanL;
  
  for (;;)
  {
    // Blocked until element is written in the queue
    xQueueReceive(xProcessingInputQueue, &input, portMAX_DELAY);
    
    switch (input.sender)
    {
      case TIMER:
        // Read data from memory
        temp = records[ri].temperature;
        lum = records[ri].luminosity;
        
        // TODO: Represent T on RGB led, L on normal leds
        
        // Increment index
        ri++;
        ri %= NR;
        break;
        
      case CONSOLE:
        Time time1 = input.interval.time1;
        Time time2 = input.interval.time2;
        
        // TODO: compute max, min, mean values from records in given interval
        
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

  // Semaphores and mutexes
  xAlarmSemaphore = xSemaphoreCreateBinary();      // used to unblock Alarm  
  xClockMutex = xSemaphoreCreateMutex();           // used for hours, minutes, seconds
  xPrintingMutex = xSemaphoreCreateMutex();        // used for lcd

  // Queues
  xSensorInputQueue = xQueueCreate(1, sizeof(Sender));
  xSensorOutputQueue = xQueueCreate(1, sizeof(Sensor));
  xProcessingInputQueue = xQueueCreate(1, sizeof(InputData));
  xProcessingOutputQueue = xQueueCreate(1, sizeof(OutputData));
  
  //TODO: check semaphore, mutexes and queues are not NULL?
  
  // Tasks (TODO: check priorities)
  
  xTaskCreate(vTaskAlarm, "Alarm", 2*configMINIMAL_STACK_SIZE, NULL, 4, NULL);
  xTaskCreate(vTaskSensorTimer, "TimerPMON", 2*configMINIMAL_STACK_SIZE, NULL, 4, &xSensorTimer);
  xTaskCreate(vTaskProcessingTimer, "TimerPPROC", 2*configMINIMAL_STACK_SIZE, NULL, 4, &xProcessingTimer);
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
