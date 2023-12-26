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
AnalogIn pot1(p19);
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
uint8_t count_to_tala = 5;            // time since buzzer started
uint8_t temp, lum;
uint8_t nr, wi, ri;                   // ring-buffer parameters
uint8_t record_nr = 0;
Record records[NR];                   // ring-buffer

/*-------------------------------------------------------------------------+
| Function: my_fgets        (called from my_getline / monitor) 
+--------------------------------------------------------------------------*/ 
char* my_fgets (char* ln, int sz, FILE* f)
{
  //fgets(line, MAX_LINE, stdin);
  //pc.gets(line, MAX_LINE);
  int i; char c;
  for(i=0; i<sz-1; i++) {
    c = pc.getc();
    ln[i] = c;
    if ((c == '\n') || (c == '\r')) break;
  }
  ln[i] = '\0';

  return ln;
}

// CLOCK
void vTaskClock(void *pvParameters)
{
  for(;;)
  {
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    lcd.locate(4,2);
    lcd.printf("%02u:%02u:%02u", hours, minutes, seconds);
    xSemaphoreGive(xPrintingMutex);
    
    // Alarm handler
    if(alaf)
    {
      if(hours == alah && minutes == alam && seconds == alas)
      {
        if (alah != 0 || alam != 0 || alas != 0)
        {
          xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
          lcd.locate(77, 2);
          lcd.printf("C");
          xSemaphoreGive(xPrintingMutex);
          count_to_tala = 0;
        }
      }
    }

    // 1 sec delay
    vTaskDelay(pdMS_TO_TICKS(1000));

    // update time (after delay to start counting at 0 and have no issues with C alarm)
    if (seconds < 59) seconds++;
    else
    {
      seconds = 0;
      if (minutes < 59)
      {
        minutes++;
      }
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
  for(;;) if(pmon != 0)
  {
    temp = (uint8_t)sensor.temp(); // TODO: better to use a float?
    lum = (uint8_t)pot1.read();

    // Print sensors' values
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    lcd.locate(4,20);
    lcd.printf("%u C ", temp); 
    lcd.locate(107, 20);
    lcd.printf("L %u", lum);
    xSemaphoreGive(xPrintingMutex);

    records[record_nr].hours = hours;
    records[record_nr].minutes = minutes;
    records[record_nr].seconds = seconds;
    records[record_nr].temperature = temp;
    records[record_nr].luminosity = lum;

    record_nr++;
    record_nr %= 20;

    if(alaf)
    {
      if(temp > alat)
      {
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(87,2);
        lcd.printf("T", temp);
        xSemaphoreGive(xPrintingMutex);
        count_to_tala = 0;
      }

      if(lum > alal)
      {
        xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
        lcd.locate(97,2);
        lcd.printf("L");
        xSemaphoreGive(xPrintingMutex);
        count_to_tala = 0;
      }
    }
    
    vTaskDelay(pdMS_TO_TICKS(1000 * pmon));
  }
}

// PWM BUZZER
void vTaskBuzzer(void *pvParameters)
{
  for(;;)
  {
    if(count_to_tala < tala)
    {
      speaker = 0.5;
      count_to_tala++;
    }
    else {
      speaker = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{
  for(;;)
  {
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    lcd.locate(117,2);
    if(alaf)
      lcd.printf("A");
    else
      lcd.printf("a");
    xSemaphoreGive(xPrintingMutex);
    
    vTaskDelay(pdMS_TO_TICKS(1000 * pproc));
  }
}

// CONSOLE
void vTaskConsole(void *pvParameters)
{
  for(;;) 
  {
    // Call console
    monitor();

    vTaskDelay(pdMS_TO_TICKS(1000)); //temporary
  }
}

int main(void) {

  pc.baud(115200);

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
  
  // Tasks (TODO: define priorities)
  xTaskCreate(vTaskClock, "Clock", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskSensors, "Sensors", 2*configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  xTaskCreate(vTaskBuzzer, "Buzzer", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskProcessing, "Processing", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskConsole, "Console", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for(;;);
  return 0;
}
