#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "LM75B.h"
#include "C12832.h"
#include "semphr.h"

#define NR 20 // maximum size of buffer

// FUNCTIONS
extern void monitor(void);

//DigitalOut led1(LED1);
C12832 lcd(p5, p7, p6, p8, p11);
LM75B sensor(p28, p27);
Serial pc(USBTX, USBRX);
AnalogIn pot1(p19);

// QUEUES
//QueueHandle_t xQueue;

// SEMAPHORES
SemaphoreHandle_t xClockMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t xParamMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t xAlarmMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t xPrintingMutex = xSemaphoreCreateMutex();
SemaphoreHandle_t xProcessingMutex = xSemaphoreCreateMutex();
// TODO: check return value of all of them?

// SHARED DATA
uint8_t hours = 0, minutes = 0, seconds = 0;
uint8_t pmon = 3, tala = 5, pproc = 0; 
uint8_t alah = 0, alam = 0, alas = 0, alat = 0, alal = 0;
bool alaf = 0; // alaf = 0 --> a, alaf = 1 --> A
uint8_t nr, wi, ri;

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t temperature;
  uint8_t luminosity;
} Record;
// ring-buffer
Record records[20];

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
    lcd.locate(2,2);
    lcd.printf("%02d:%02d:%02d", hours, minutes, seconds);
    xSemaphoreGive(xPrintingMutex);

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

    // TODO: handle alarm here

    vTaskDelay(pdMS_TO_TICKS(1000)); // 1 sec delay
  }
}

// SENSORS
void vTaskSensors(void *pvParameters)
{
  for(;;)
  {
    xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
    lcd.locate(2,20);
    lcd.printf("%u C ", (uint8_t)sensor.temp());  // TODO: only read here, values are displayed in TaskInterface
    lcd.locate(100, 20);
    lcd.printf("L %u", (uint8_t)pot1.read());     // TODO: fix this read
    xSemaphoreGive(xPrintingMutex);
    
    vTaskDelay(pdMS_TO_TICKS(3000));
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{
  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));    //temporary
  }
}

// CONSOLE
void vTaskInterface(void *pvParameters)
{
  for(;;) 
  {
    //lValueToSend = 100;
    //xStatus = xQueueSend(xQueue, &lValueToSend, 0);
    monitor(); //does not return
    vTaskDelay(pdMS_TO_TICKS(1000));    //temporary
  }
}

int main(void) {

  pc.baud(115200);

  // --- APPLICATION TASKS CAN BE CREATED HERE ---

  // Queues
  //xQueue = xQueueCreate(3, sizeof(int32_t));
  
  // Tasks
  xTaskCreate(vTaskClock, "Clock", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskSensors, "Sensors", 2*configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  xTaskCreate(vTaskProcessing, "Processing", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskInterface, "UserInterface", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for(;;);
  return 0;
}
