#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "LM75B.h"
#include "C12832.h"

//DigitalOut led1(LED1);
C12832 lcd(p5, p7, p6, p8, p11);
LM75B sensor(p28, p27);
Serial pc(USBTX, USBRX);

//QueueHandle_t xQueue;

extern void monitor(void);
       void show_temp(void);

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
  for(;;)
  {
    show_temp();
  }
}

// PROCESSING
void vTaskProcessing(void *pvParameters)
{

}

// CONSOLE
void vTaskConsole(void *pvParameters)
{
  for(;;) 
  {
    //lValueToSend = 100;
    //xStatus = xQueueSend(xQueue, &lValueToSend, 0);
    monitor(); //does not return
  }
}

int main(void) {

  pc.baud(115200);

  // --- APPLICATION TASKS CAN BE CREATED HERE ---

  // Queues
  //xQueue = xQueueCreate(3, sizeof(int32_t));
  
  // Tasks
  xTaskCreate(vTaskClock, "Clock", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskSensors, "Sensors", 2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
  xTaskCreate(vTaskProcessing, "Processing", 2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  xTaskCreate(vTaskConsole, "Console", 2*configMINIMAL_STACK_SIZE, NULL, 2, NULL);
  
  // Start the created tasks running
  vTaskStartScheduler();

  // Execution will only reach here if there was insufficient heap to start the scheduler
  for(;;);
  return 0;
}

void show_temp(void)
{
  lcd.cls();
  lcd.locate(0,3);
  lcd.printf("Temp = %.3f\n", (float)sensor.temp());
}