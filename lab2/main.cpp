#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "LM75B.h"
#include "C12832.h"
#include "semphr.h"
#include "shared.h" // custom header for structs

// FUNCTIONS
extern void monitor(void);

//DigitalOut led1(LED1);
C12832 lcd(p5, p7, p6, p8, p11);
LM75B sensor(p28, p27);
Serial pc(USBTX, USBRX);
AnalogIn pot1(p19); // TODO: fix potentiometer
PwmOut speaker(p26);

// QUEUES
QueueHandle_t xSensorQueue, xProcessingInputQueue, xProcessingOutputQueue;

// SEMAPHORES
SemaphoreHandle_t xClockMutex, xPrintingMutex, xParamMutex, xAlarmMutex, xProcessingMutex;

// SHARED DATA
uint8_t hours = 0, minutes = 0, seconds = 0;
uint8_t pmon = 3, tala = 5, pproc = 0; 
uint8_t alah = 0, alam = 0, alas = 0; // keeping default values means no C alarm
uint8_t alat = 20, alal = 2;
bool alaf = 0;                        // alaf = 0 --> a, alaf = 1 --> A
uint8_t tala_count = 0;               // time left for the buzzer to stop ringing (tala seconds after the start)
uint8_t temp, lum;                    // sensors' values
uint8_t nr, wi, ri;                   // ring-buffer parameters
uint8_t record_nr = 0;                // current index
Record records[NR];                   // ring-buffer

// BUZZER
void vTaskAlarm(void *pvParameters) // TODO: make it stop ringing after tala even when the alarm is still present?
{
  for (;;) 
  {
    /*
    ALTERNATIVE IDEA: keep this task blocked if no alarm condition. If unblocked, use delay for controlling the time.
    */
    
    // Handle buzzer (TODO: problem is that buzzer will not start immediatley, only at following execution of this task)
    if (tala_count != 0)
    {
      speaker = 0.5;
      tala_count--;
    }
    else
      speaker = 0;
    
    // 1 sec delay (TODO: check if 1s is enough for the other tasks to execute, if not change approach)
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// CLOCK
void vTaskClock(void *pvParameters)
{
  for (;;)
  {
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
          xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
          lcd.locate(77, 2);
          lcd.printf("C");
          xSemaphoreGive(xPrintingMutex);
          tala_count = tala; // start buzzer (TODO: change approach to a blocking one?)
        }
      }
    }

    // 1 sec delay
    vTaskDelay(pdMS_TO_TICKS(1000));

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
  }
}

// SENSORS
void vTaskSensors(void *pvParameters)
{
  for (;;) if (pmon != 0)
  {
    temp = (uint8_t)sensor.temp(); // TODO: better to use a float?
    lum = (uint8_t)pot1.read();

    // Print sensors' values
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    lcd.locate(4,20);    // temperature
    lcd.printf("%u C ", temp); 
    lcd.locate(107, 20); // luminosity
    lcd.printf("L %u", lum);
    xSemaphoreGive(xPrintingMutex);
    
    // Save record
    records[record_nr].hours = hours;
    records[record_nr].minutes = minutes;
    records[record_nr].seconds = seconds;
    records[record_nr].temperature = temp;
    records[record_nr].luminosity = lum;
    // Increment index
    record_nr++;
    record_nr %= 20;

    // Handle alarm
    if (alaf)
    {
      if (temp > alat)
      {
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(87,2);
        lcd.printf("T", temp);
        xSemaphoreGive(xPrintingMutex);
        tala_count = tala; // start buzzer (TODO: change approach to a blocking one?)
      }
      
      if (lum > alal)
      {
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(97,2);
        lcd.printf("L");
        xSemaphoreGive(xPrintingMutex);
        tala_count = tala; // start buzzer (TODO: change approach to a blocking one?)
      }
    }
    
    // PMON sec delay
    vTaskDelay(pdMS_TO_TICKS(1000 * pmon));
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{
  for (;;) if (pproc != 0)
  {
    // TODO: implement features 
    
    // PPROC sec delay
    vTaskDelay(pdMS_TO_TICKS(1000 * pproc));
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

  // Semaphores
  xClockMutex = xSemaphoreCreateMutex();
  xParamMutex = xSemaphoreCreateMutex();
  xAlarmMutex = xSemaphoreCreateMutex();
  xPrintingMutex = xSemaphoreCreateMutex();
  xProcessingMutex = xSemaphoreCreateMutex();
  // TODO: check return value of all of them?

  // Queues
  xSensorQueue = xQueueCreate(1, sizeof(Sensor));
  xProcessingInputQueue = xQueueCreate(1, sizeof(Interval));
  xProcessingOutputQueue = xQueueCreate(1, sizeof(Process));
  
  // Tasks (TODO: check priorities)
  xTaskCreate(vTaskClock, "Clock", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskSensors, "Sensors", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskAlarm, "Alarm", 2*configMINIMAL_STACK_SIZE, NULL, 4, NULL);
  xTaskCreate(vTaskProcessing, "Processing", 2*configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  xTaskCreate(vTaskConsole, "Console", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for (;;);
  return 0;
}
