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

extern QueueHandle_t xSensorQueue, xProcessingQueue, xProcessingInputQueue, xProcessingOutputQueue;

extern SemaphoreHandle_t xClockMutex, xPrintingMutex;

extern uint8_t hours, minutes, seconds;
extern uint8_t pmon, tala, pproc; 
extern uint8_t alah, alam, alas, alat, alal;
extern bool alaf;
extern uint8_t nr, wi, ri;
extern Record records[NR];

const Time invalid = {INVALID, INVALID, INVALID};

bool checkTime(Time *time);
bool compareTime(Time *time1, Time *time2);

/*-------------------------------------------------------------------------+
| Function: cmd_rc  - read clock
+--------------------------------------------------------------------------*/ 
void cmd_rc (int argc, char** argv) 
{
  // Mutex not used to display latest time
  printf("\nCurrent clock: %02d:%02d:%02d", hours, minutes, seconds);
}
/*-------------------------------------------------------------------------+
| Function: cmd_sc  - set clock
+--------------------------------------------------------------------------*/ 
void cmd_sc (int argc, char** argv) 
{
  if (argc == 4)
  {
    Time time;
    time.hours = atoi(argv[1]);
    time.minutes = atoi(argv[2]);
    time.seconds = atoi(argv[3]);
    if (checkTime(&time))
    {
      // CRITICAL SECTION: hours, minutes, seconds may be modified from TaskClock
      xSemaphoreTake(xClockMutex, portMAX_DELAY);
      hours = (uint8_t)time.hours;
      minutes = (uint8_t)time.minutes;
      seconds = (uint8_t)time.seconds;
      xSemaphoreGive(xClockMutex);
      
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
  Sensor values;

  // TODO: execute TaskSensor

  if (xQueueReceive(xSensorQueue, &values, portMAX_DELAY) == pdPASS)
    // Display read values
    printf("\nTemperature = %u °C, Luminosity = %u\n", values.temp, values.lum);
}
/*-------------------------------------------------------------------------+
| Function: cmd_rp  - read parameters (pmon, tala, pproc)
+--------------------------------------------------------------------------*/ 
void cmd_rp (int argc, char** argv) 
{
  // Mutex not needed because parameters are written only in this task
  printf("\nPMON = %u, TALA = %u, PPROC = %u seconds\n", pmon, tala, pproc);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mmp - modify monitoring period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mmp (int argc, char** argv) 
{
  if (argc == 2)
  {
    short s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      // Mutex not needed if we accept the modification to be available at the latest on the second execution of TaskSensors
      pmon = (uint8_t)s;
      printf("\nMonitoring period correctly set!\n");
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
      // Mutex not needed if we accept the modification to be available at the latest on the second execution of TaskClock or TaskSensors
      tala = (uint8_t)s;
      printf("\nMonitoring period correctly set!\n");
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
  if (argc == 2)
  {
    short s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      // Mutex not needed if we accept the modification to be available at the latest on the second execution of TaskProcessing
      pproc = (uint8_t)s;
      printf("\nMonitoring period correctly set!\n");
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
  printf("\nALAH = %u, ALAM = %u, ALAS = %u\n", alah, alam, alas);
  printf("ALAT = %u, ALAL = %u, ALAF = %c\n", alat, alal, alaf ? 'A' : 'a');
}
/*-------------------------------------------------------------------------+
| Function: cmd_dac - define alarm clock
+--------------------------------------------------------------------------*/ 
void cmd_dac (int argc, char** argv) 
{
  if (argc == 4)
  {
    Time time;
    time.hours = atoi(argv[1]);
    time.minutes = atoi(argv[2]);
    time.seconds = atoi(argv[3]);
    if (checkTime(&time))
    {
      // Mutex not needed if we accept the modification to be available at the latest on the second execution of TaskClock
      alah = (uint8_t)time.hours;
      alam = (uint8_t)time.minutes;
      alas = (uint8_t)time.seconds;
      printf("\nClock correctly set!\n");
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
    if (t >= 0 && t <= 50) // check temperature
    {
      if (l >= 0 && l <= 3) // check luminosity
      {
        // Mutex not needed if we accept the modification to be available at the latest on the second execution of TaskSensors
        alat = (uint8_t)t;
        alal = (uint8_t)l;
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
    short num = atoi(argv[1]);
    if (num == 65 || num == 97) // ASCII: A = 65, a = 97
    {
      // Mutex not needed if we accept the modification to be available at the latest on the second execution of TaskClock or TaskSensors
      alaf = (num == 65) ? 1 : 0;
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
  // CRITICAL SECTION: TaskClock and TaskSensors may want to use display
  xSemaphoreTake(xPrintingMutex, portMAX_DELAY);
  lcd.locate(77, 2); // C
  lcd.printf(" ");
  lcd.locate(87, 2); // T
  lcd.printf(" ");
  lcd.locate(97, 2); // L
  lcd.printf(" ");
  xSemaphoreGive(xPrintingMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_ir  - information about records (NR, nr, wi, ri)
+--------------------------------------------------------------------------*/ 
void cmd_ir (int argc, char** argv) 
{
  // Mutex not used to display latest parameters
  printf("\nNR = %u, nr = %u, wi = %u, ri = %u\n", NR, nr, wi, ri);
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
        for (short j = i; j < n; j++)
          printf("\nRecord %hd: %02u:%02u:%02u %u °C, L %u\n", j, records[j].hours, records[j].minutes, records[j].seconds, records[j].temperature, records[j].luminosity);
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
  for (uint8_t i = 0; i < NR; i++)
  {
    records[i].hours = 0;
    records[i].minutes = 0;
    records[i].seconds = 0;
    records[i].temperature = 0;
    records[i].luminosity = 0;
  }
}
/*-------------------------------------------------------------------------+
| Function: cmd_pr  - process records (max, min, mean) between instants t1 and t2 (h,m,s)
+--------------------------------------------------------------------------*/ 
void cmd_pr (int argc, char** argv) 
{
  Time time1, time2;
  Interval interval;
  Process process;

  switch (argc)
  {
    case 1:
      interval.time1 = invalid;
      interval.time2 = invalid;
      // Send data
      xQueueSend(xProcessingInputQueue, (void*)&interval, portMAX_DELAY);
      // Receive data
      if (xQueueReceive(xProcessingOutputQueue, &process, portMAX_DELAY) == pdPASS)
      {
        printf("\nTemperature (max, min, mean) = %u, %u, %f", process.maxT, process.minT, process.meanT);
        printf("\nLuminosity (max, min, mean) = %u, %u, %f\n", process.maxL, process.minL, process.meanL);
      }
      break;

    case 4:
      time1.hours = atoi(argv[1]); 
      time1.minutes = atoi(argv[2]); 
      time1.seconds = atoi(argv[3]);
      if (checkTime(&time1))
      {
        interval.time1 = time1;
        interval.time2 = invalid;
        // Send data
        xQueueSend(xProcessingInputQueue, (void*)&interval, portMAX_DELAY);
        // Receive data
        if (xQueueReceive(xProcessingOutputQueue, &process, portMAX_DELAY) == pdPASS)
        {
          printf("\nTemperature (max, min, mean) = %u, %u, %f", process.maxT, process.minT, process.meanT);
          printf("\nLuminosity (max, min, mean) = %u, %u, %f\n", process.maxL, process.minL, process.meanL);
        }
      }
      else printf("\nInvalid time format!\n");
      break;

    case 7:
      time1.hours = atoi(argv[1]); 
      time1.minutes = atoi(argv[2]); 
      time1.seconds = atoi(argv[3]);
      time2.hours = atoi(argv[4]); 
      time2.minutes = atoi(argv[5]); 
      time2.seconds = atoi(argv[6]);
      if (checkTime(&time1) && checkTime(&time2))
      {
        if (compareTime(&time1, &time2))
        {
          interval.time1 = time1;
          interval.time2 = time2;
          // Send data
          xQueueSend(xProcessingInputQueue, (void*)&interval, portMAX_DELAY);
          // Receive data
          if (xQueueReceive(xProcessingOutputQueue, &process, portMAX_DELAY) == pdPASS)
          {
            printf("\nTemperature (max, min, mean) = %u, %u, %f", process.maxT, process.minT, process.meanT);
            printf("\nLuminosity (max, min, mean) = %u, %u, %f\n", process.maxL, process.minL, process.meanL);
          }
        }
        else printf("\nInvalid time interval!\n");
      }
      else printf("\nInvalid time format\n");
      break;

    default: printf("\nInvalid number of arguments!\n");
  }
}

// --- UTILITY ---

bool checkTime(Time *time)
{
  if (time->hours < 0 || time->hours > 23) return false;
  if (time->minutes < 0 || time->minutes > 59) return false;
  if (time->seconds < 0 || time->seconds > 59) return false;
  return true;
}

bool compareTime(Time *time1, Time *time2)
{
  // Convert times to seconds since midnight
  uint32_t sec1 = time1->hours * 3600 + time1->minutes * 60 + time1->seconds;
  uint32_t sec2 = time2->hours * 3600 + time2->minutes * 60 + time2->seconds;
  return (sec2 > sec1);
}