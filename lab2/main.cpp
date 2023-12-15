#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

//DigitalOut led1(LED1);
Serial pc(USBTX, USBRX);

//QueueHandle_t xQueue;

extern void monitor(void);

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

}

// SENSORS
void vTaskSensors(void *pvParameters)
{

}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{

}

// CONSOLE
void vTaskConsole(void *pvParameters)
{
  for(;;) {
    //lValueToSend = 100;
    //xStatus = xQueueSend(xQueue, &lValueToSend, 0);
    monitor(); //does not return
  }
}

int main(void) {

  // Perform any hardware setup necessary
  //prvSetupHardware();

  pc.baud(115200);

  // --- APPLICATION TASKS CAN BE CREATED HERE ---

  //xQueue = xQueueCreate(3, sizeof(int32_t));
  
  xTaskCreate(vTaskClock, "Clock", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskSensors, "Sensors", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskProcessing, "Processing", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskConsole, "Console", 2*configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for(;;);
  return 0;
}