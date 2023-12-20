#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "LM75B.h"
#include "C12832.h"
#include "semphr.h"

//DigitalOut led1(LED1);
C12832 lcd(p5, p7, p6, p8, p11);
LM75B sensor(p28, p27);
Serial pc(USBTX, USBRX);

//QueueHandle_t xQueue;

extern void monitor(void);
       void show_temp(void);

SemaphoreHandle_t printing_mutex = xSemaphoreCreateMutex();

unsigned hours = 0, minutes = 0, seconds = 0;

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
    xSemaphoreTake(printing_mutex, portMAX_DELAY);
    lcd.locate(2,2);
    lcd.printf("%02d:%02d:%02d", hours, minutes, seconds);
    xSemaphoreGive(printing_mutex);

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

    vTaskDelay( pdMS_TO_TICKS( 1000 ) );
  }
}

// SENSORS
void vTaskSensors(void *pvParameters)
{
  for(;;)
  {
    xSemaphoreTake(printing_mutex, portMAX_DELAY);
    lcd.locate(2,19);
    lcd.printf("%d C ", (int)sensor.temp());
    //lcd.locate(30, 19);
    //lcd.printf("L %d", (int)sensor.read8(p21));
    xSemaphoreGive(printing_mutex);
    
    vTaskDelay( pdMS_TO_TICKS( 3000 ) );
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{
  for(;;)
  {
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );    //temporary
  }
}

// CONSOLE
void vTaskConsole(void *pvParameters)
{
  for(;;) 
  {
    //lValueToSend = 100;
    //xStatus = xQueueSend(xQueue, &lValueToSend, 0);
    monitor(); //does not return
    vTaskDelay( pdMS_TO_TICKS( 1000 ) );    //temporary
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
  xTaskCreate(vTaskConsole, "Console", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for(;;);
  return 0;
}
