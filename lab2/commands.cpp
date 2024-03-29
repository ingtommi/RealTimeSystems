#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "portmacro.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"
#include "C12832.h"
#include "shared.h" // custom header for shared objects

extern C12832 lcd;
extern PwmOut r, b;
extern BusOut leds;

extern TaskHandle_t xSensorTimer, xProcessingTimer;

extern QueueHandle_t xSensorInputQueue, xSensorOutputQueue, xProcessingQueue, xProcessingInputQueue, xProcessingOutputQueue;

extern SemaphoreHandle_t xClockMutex, xPrintingMutex, xBufferMutex, xAlarmMutex, xParamMutex;

extern uint8_t hours, minutes, seconds;
extern uint8_t pmon, tala, pproc; 
extern uint8_t alah, alam, alas, alat, alal;
extern bool alaf;
extern uint8_t nr, wi, ri;
extern Record records[NR];
const Time invalid = {INVALID, INVALID, INVALID};

bool splitTime(char *arg, Time *time);
bool compareTime(Time *time1, Time *time2);

/*-------------------------------------------------------------------------+
| Function: cmd_rc  - read clock
+--------------------------------------------------------------------------*/ 
void cmd_rc (int argc, char** argv) 
{
  // CRITICAL SECTION
  xSemaphoreTake(xClockMutex, portMAX_DELAY);
  printf("\nCurrent clock: %02d:%02d:%02d\n", hours, minutes, seconds);
  xSemaphoreGive(xClockMutex);
  // END OF CRITICAL SECTION
}
/*-------------------------------------------------------------------------+
| Function: cmd_sc  - set clock
+--------------------------------------------------------------------------*/ 
void cmd_sc (int argc, char** argv) 
{
  if (argc == 2)
  {
    Time time;
    if (splitTime(argv[1], &time)) // split hh:mm:ss and check if time is consistent
    {
      // CRITICAL SECTION
      xSemaphoreTake(xClockMutex, portMAX_DELAY);
      hours = (uint8_t)time.hours;
      minutes = (uint8_t)time.minutes;
      seconds = (uint8_t)time.seconds;
      xSemaphoreGive(xClockMutex);
      //END OF CRITICAL SECTION
      printf("\nClock correctly set!\n");
    }
    else printf("\nInvalid time format!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_rtl - read temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_rtl (int argc, char** argv) 
{
  Sender sender = CONSOLE;
  Sensor values;

  // Unblock TaskSensors
  xQueueSend(xSensorInputQueue, (void*)&sender, portMAX_DELAY);
  // Receive data (returned value not checked because portMAX_DELAY is used)
  xQueueReceive(xSensorOutputQueue, &values, portMAX_DELAY);
  // Display read values
  printf("\nTemperature = %u °C, Luminosity = %u\n", values.temp, values.lum);
}
/*-------------------------------------------------------------------------+
| Function: cmd_rp  - read parameters (pmon, tala, pproc)
+--------------------------------------------------------------------------*/ 
void cmd_rp (int argc, char** argv) 
{
  // CRITICAL SECTION
  xSemaphoreTake(xParamMutex, portMAX_DELAY);
  printf("\nPMON = %u, TALA = %u, PPROC = %u seconds\n", pmon, tala, pproc);
  xSemaphoreGive(xParamMutex);
  // END OF CRITICAL SECTION
}
/*-------------------------------------------------------------------------+
| Function: cmd_mmp - modify monitoring period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mmp (int argc, char** argv) 
{
  eTaskState TimerState = eTaskGetState(xSensorTimer);
  
  if (argc == 2)
  {
    short s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      // CRITICAL SECTION: change priority to avoid this task being preempted before suspending/resuming the timer
      // (The mutex will only guarantee pmon will not be modified by others, not correct suspend/resume)
      vTaskPrioritySet(NULL, 4); // NULL refers to current task
      // CRITICAL SECTION
      xSemaphoreTake(xParamMutex, portMAX_DELAY);
      pmon = (uint8_t)s;
      // Suspend TaskSensorTimer if pmon is 0
      if (pmon == 0)
        vTaskSuspend(xSensorTimer);
      // Resume TaskSensorTimer if pmon is not 0 and task was previously suspended
      else
      {
        if (TimerState == eSuspended)
          vTaskResume(xSensorTimer);
      }
      xSemaphoreGive(xParamMutex);
      printf("\nMonitoring period correctly set!\n");
      vTaskPrioritySet(NULL, 1);
      // END OF CRITICAL SECTION
    }
    else printf("\nInvalid seconds!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_mta - modify time alarm (seconds)
+--------------------------------------------------------------------------*/ 
void cmd_mta (int argc, char** argv) 
{
  if (argc == 2)
  {
    short s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      // CRITICAL SECTION
      xSemaphoreTake(xParamMutex, portMAX_DELAY);
      tala = (uint8_t)s;
      xSemaphoreGive(xParamMutex);
      // END OF CRITICAL SECTION
      printf("\nAlarm time correctly set!\n");
    }
    else printf("\nInvalid seconds!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_mpp - modify processing period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mpp (int argc, char** argv) 
{
  eTaskState TimerState = eTaskGetState(xProcessingTimer);
  
  if (argc == 2)
  {
    short s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      // CRITICAL SECTION: change priority to avoid this task being preempted before suspending/resuming the timer
      // (The mutex will only guarantee pproc will not be modified by others, not correct suspend/resume)
      //vTaskPrioritySet(NULL, 4); // NULL refers to current task
      xSemaphoreTake(xParamMutex, portMAX_DELAY);
      pproc = (uint8_t)s;
      // Suspend TaskProcessingTimer if pproc is 0
      if (pproc == 0)
      {
        vTaskSuspend(xProcessingTimer);
        // Turn off leds
        leds = 0x0;   
        r = 1; b = 1;
      }
      // Resume TaskProcessingTimer if pproc is not 0 and task was previously suspended
      else
      {
        if (TimerState == eSuspended)
          vTaskResume(xProcessingTimer);
      }
      xSemaphoreGive(xParamMutex);
      printf("\nMonitoring period correctly set!\n");
      //vTaskPrioritySet(NULL, 4);
      // END OF CRITICAL SECTION
    }
    else printf("\nInvalid seconds!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_rai - read alarm info (clock, temperature, luminosity, active/inactive-A/a)
+--------------------------------------------------------------------------*/ 
void cmd_rai (int argc, char** argv) 
{
  // CRITICAL SECTION
  xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
  printf("\nALAH = %u, ALAM = %u, ALAS = %u\n", alah, alam, alas);
  printf("ALAT = %u, ALAL = %u, ALAF = %c\n", alat, alal, alaf ? 'A' : 'a');
  xSemaphoreGive(xAlarmMutex);
  // END OF CRITICAL SECTION
}
/*-------------------------------------------------------------------------+
| Function: cmd_dac - define alarm clock
+--------------------------------------------------------------------------*/ 
void cmd_dac (int argc, char** argv) 
{
  if (argc == 2)
  {
    Time time;
    if (splitTime(argv[1], &time)) // split hh:mm:ss and check if time is consistent
    {
      // CRITICAL SECTION
      xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
      alah = (uint8_t)time.hours;
      alam = (uint8_t)time.minutes;
      alas = (uint8_t)time.seconds;
      xSemaphoreGive(xAlarmMutex);
      // END OF CRITICAL SECTION
      printf("\nClock threshold correctly set!\n");
    }
    else printf("\nInvalid time format!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_dtl - define alarm temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_dtl (int argc, char** argv) 
{
  if (argc == 3)
  {
    short t = atoi(argv[1]), l = atoi(argv[2]);
    if (t >= 0 && t <= 50)  // check temperature
    {
      if (l >= 0 && l <= 3) // check luminosity
      {
        // CRITICAL SECTION
        xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
        alat = (uint8_t)t;
        alal = (uint8_t)l;
        xSemaphoreGive(xAlarmMutex);
        // END OF CRITICAL SECTION
        printf("\nSensor thresholds correctly set!\n");
      }
      else printf("\nInvalid luminosity!\n");
    }
    else printf("\nInvalid temperature!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_aa  - activate/deactivate alarms (A/a)
+--------------------------------------------------------------------------*/ 
void cmd_aa (int argc, char** argv) 
{
  if (argc == 2)
  {
    int num = int(argv[1][0]);
    if (num == 65 || num == 97) // ASCII: A = 65, a = 97
    {
      // CRITICAL SECTION
      xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
      alaf = (num == 65) ? 1 : 0;
      xSemaphoreGive(xAlarmMutex);
      // END OF CRITICAL SECTION
      printf("\nAlarm mode correctly set!\n");
    }
    else printf("\nInvalid character!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_cai - clear alarm info (letters CTL in LCD)
+--------------------------------------------------------------------------*/ 
void cmd_cai (int argc, char** argv) 
{
  // CRITICAL SECTION
  xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
  lcd.locate(77, 2); // C
  lcd.printf(" ");
  lcd.locate(87, 2); // T
  lcd.printf(" ");
  lcd.locate(97, 2); // L
  lcd.printf(" ");
  xSemaphoreGive(xPrintingMutex);
  // END OF CRITICAL SECTION
  printf("\nAlarm correctly cleared!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_ir  - information about records (NR, nr, wi, ri)
+--------------------------------------------------------------------------*/ 
void cmd_ir (int argc, char** argv) 
{
  // CRITICAL SECTION
  xSemaphoreTake(xBufferMutex, portMAX_DELAY);
  printf("\nNR = %u, nr = %u, wi = %u, ri = %u\n", NR, nr, wi, ri);
  xSemaphoreGive(xBufferMutex);
  // END OF CRITICAL SECTION
}
/*-------------------------------------------------------------------------+
| Function: cmd_lr  - list n records from index i (0 - oldest)
+--------------------------------------------------------------------------*/ 
void cmd_lr (int argc, char** argv) 
{
  if (argc == 3)
  {
    short n = atoi(argv[1]), i = atoi(argv[2]);
    if (n >= 0 && n <= NR) // check n
    {
      if (i >= 0 && i < NR) // check i
      {
        // CRITICAL SECTION
        xSemaphoreTake(xBufferMutex, portMAX_DELAY);
        for (short j = i; j < n; j++)
          printf("\nRecord %hd: %02u:%02u:%02u %u°C, %u\n", j, records[j].hours, records[j].minutes, records[j].seconds, records[j].temperature, records[j].luminosity);
        xSemaphoreGive(xBufferMutex);
        // END OF CRITICAL SECTION
      }
      else printf("\nInvalid number of records!\n");
    }
    else printf("\nInvalid index!\n");
  }
  else printf("\nInvalid number of arguments!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_dr  - delete records
+--------------------------------------------------------------------------*/ 
void cmd_dr (int argc, char** argv) 
{
  // CRITICAL SECTION
  xSemaphoreTake(xBufferMutex, portMAX_DELAY);
  // Delete records
  for (uint8_t i = 0; i < NR; i++)
  {
    records[i].hours = 0;
    records[i].minutes = 0;
    records[i].seconds = 0;
    records[i].temperature = 0;
    records[i].luminosity = 0;
  }
  // Clear parameters
  nr = 0;
  wi = 0;
  ri = 0;
  xSemaphoreGive(xBufferMutex);
  // END OF CRITICAL SECTION
  printf("\nRecord correctly deleted!\n");
}
/*-------------------------------------------------------------------------+
| Function: cmd_pr  - process records (max, min, mean) between instants t1 and t2 (h,m,s)
+--------------------------------------------------------------------------*/ 
void cmd_pr (int argc, char** argv) 
{
  Time time1, time2;
  Interval interval;
  Sender sender = CONSOLE;
  InputData input;
  OutputData output;

  switch (argc)
  {
    case 1:
      interval.time1 = invalid;
      interval.time2 = invalid;
      input.interval = interval; input.sender = sender;
      // Send data
      xQueueSend(xProcessingInputQueue, (void*)&input, portMAX_DELAY);
      // Receive data
      xQueueReceive(xProcessingOutputQueue, &output, portMAX_DELAY);
      if (output.minT == 50)
        printf("\nNo records to be read!\n");
      else
      {
        printf("\nTemperature (max, min, mean) = %u, %u, %.1f", output.maxT, output.minT, output.meanT);
        printf("\nLuminosity (max, min, mean) = %u, %u, %.1f\n", output.maxL, output.minL, output.meanL);
      }
      break;

    case 2:
      if (splitTime(argv[1], &time1)) // split hh:mm:ss and check if time is consistentif (splitTime(argv[1], &time)) // split hh:mm:ss and check if time is consistent
      {
        interval.time1 = time1;
        interval.time2 = invalid;
        input.interval = interval; input.sender = sender;
        // Send data
        xQueueSend(xProcessingInputQueue, (void*)&input, portMAX_DELAY);
        // Receive data
        xQueueReceive(xProcessingOutputQueue, &output, portMAX_DELAY);
        if (output.minT == 50)
          printf("\nNo records to be read!\n");
        else
        {
          printf("\nTemperature (max, min, mean) = %u, %u, %.1f", output.maxT, output.minT, output.meanT);
          printf("\nLuminosity (max, min, mean) = %u, %u, %.1f\n", output.maxL, output.minL, output.meanL);
        }
      }
      else printf("\nInvalid time format!\n");
      break;

    case 3:
      if (splitTime(argv[1], &time1) && splitTime(argv[2], &time2)) // split hh:mm:ss and check if time is consistent
      {
        if (compareTime(&time1, &time2))
        {
          interval.time1 = time1;
          interval.time2 = time2;
          input.interval = interval; input.sender = sender;
          // Send data
          xQueueSend(xProcessingInputQueue, (void*)&input, portMAX_DELAY);
          // Receive data
          xQueueReceive(xProcessingOutputQueue, &output, portMAX_DELAY);
          if (output.minT == 50)
            printf("\nNo records to be read!\n");
          else
          {
            printf("\nTemperature (max, min, mean) = %u, %u, %.1f", output.maxT, output.minT, output.meanT);
            printf("\nLuminosity (max, min, mean) = %u, %u, %.1f\n", output.maxL, output.minL, output.meanL);
          }
        }
        else printf("\nInvalid time interval!\n");
      }
      else printf("\nInvalid time format\n");
      break;

    default: printf("\nInvalid number of arguments!\n");
  }
}
/*-------------------------------------------------------------------------+
| UTILITY
+--------------------------------------------------------------------------*/ 
bool splitTime(char *arg, Time *time)
{
  // arg must be in format "hh:mm:ss"
  if (strlen(arg) != 8 || arg[2] != ':' || arg[5] != ':') // check if length != 8 or wrong format
    return false;
  else
  {
    char *token = strtok(arg, ":");
    time->hours = atoi(token);
    if (time->hours < 0 || time->hours > 23) return false;
    token = strtok(NULL, ":");
    time->minutes = atoi(token);
    if (time->minutes < 0 || time->minutes > 59) return false;
    token = strtok(NULL, ":");
    time->seconds = atoi(token);
    if (time->seconds < 0 || time->seconds > 59) return false;
  }
  return true;
}

bool compareTime(Time *time1, Time *time2)
{
  // Convert times to seconds since midnight
  uint32_t sec1 = time1->hours * 3600 + time1->minutes * 60 + time1->seconds;
  uint32_t sec2 = time2->hours * 3600 + time2->minutes * 60 + time2->seconds;
  return (sec2 > sec1);
}
