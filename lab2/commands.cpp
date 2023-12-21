#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

#define NR 20

//extern QueueHandle_t xQueue;

extern SemaphoreHandle_t xClockMutex, xParamMutex, xAlarmMutex, xProcessingMutex, xPrintingMutex;

extern uint8_t hours, minutes, seconds;
extern uint8_t pmon, tala, pproc; 
extern uint8_t alah, alam, alas, alat, alal;
extern bool alaf; // alaf = 0 --> a, alaf = 1 --> A
extern uint8_t nr, wi, ri;

typedef struct 
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} Time;

typedef struct 
{
  uint8_t n;
  uint8_t i;
} RecordsList;

bool checkTime(Time *time);
bool compareTime(Time *time1, Time *time2);

/*-------------------------------------------------------------------------+
| Function: cmd_rc  - read clock
+--------------------------------------------------------------------------*/ 
void cmd_rc (int argc, char** argv) 
{
  xSemaphoreTake(xClockMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nCurrent clock: %02d:%02d:%02d", hours, minutes, seconds);

  xSemaphoreGive(xClockMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_sc  - set clock
+--------------------------------------------------------------------------*/ 
void cmd_sc (int argc, char** argv) 
{
  xSemaphoreTake(xClockMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 4)
  {
    Time time;
    time.hours = atoi(argv[1]);
    time.minutes = atoi(argv[2]);
    time.seconds = atoi(argv[3]);
    if (checkTime(&time))
    {
      hours = time.hours;
      minutes = time.minutes;
      seconds = time.seconds;
      printf("\nClock correctly set!\n");
    }
    else printf("\nInvalid time format!\n");
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xClockMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_rtl - read temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_rtl (int argc, char** argv) 
{
  // TODO: ask TaskSensor to read sensors and read queue to print current T, L on the console
}
/*-------------------------------------------------------------------------+
| Function: cmd_rp  - read parameters (pmon, tala, pproc)
+--------------------------------------------------------------------------*/ 
void cmd_rp (int argc, char** argv) 
{
  xSemaphoreTake(xParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nPMON = %u, TALA = %u, PPROC = %u seconds\n", pmon, tala, pproc);

  xSemaphoreGive(xParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mmp - modify monitoring period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mmp (int argc, char** argv) 
{
  xSemaphoreTake(xParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    uint8_t s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      pmon = s;
      printf("\nMonitoring period correctly set!\n");
    }
    else printf("\nInvalid seconds!\n");
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mta - modify time alarm (seconds)
+--------------------------------------------------------------------------*/ 
void cmd_mta (int argc, char** argv) 
{
  xSemaphoreTake(xParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    uint8_t s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      tala = s;
      printf("\nMonitoring period correctly set!\n");
    }
    else printf("\nInvalid seconds!\n");
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mpp - modify processing period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mpp (int argc, char** argv) 
{
  xSemaphoreTake(xParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    uint8_t s = atoi(argv[1]);
    if (s >= 0 && s < 60) // check seconds
    {
      pproc = s;
      printf("\nMonitoring period correctly set!\n");
    }
    else printf("\nInvalid seconds!\n");
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_rai - read alarm info (clock, temperature, luminosity, active/inactive-A/a)
+--------------------------------------------------------------------------*/ 
void cmd_rai (int argc, char** argv) 
{
  xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nALAH = %u, ALAM = %u, ALAS = %u\n", alah, alam, alas);
  printf("ALAT = %u, ALAL = %u, ALAF = %c\n", alat, alal, alaf ? 'A' : 'a');

  xSemaphoreGive(xAlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_dac - define alarm clock
+--------------------------------------------------------------------------*/ 
void cmd_dac (int argc, char** argv) 
{
  xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 4)
  {
    Time time;
    time.hours = atoi(argv[1]);
    time.minutes = atoi(argv[2]);
    time.seconds = atoi(argv[3]);
    if (checkTime(&time))
    {
      alah = time.hours;
      alam = time.minutes;
      alas = time.seconds;
      printf("\nClock correctly set!\n");
    }
    else printf("\nInvalid time format!\n")
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xAlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_dtl - define alarm temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_dtl (int argc, char** argv) 
{
  xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 3)
  {
    uint8_t t = atoi(argv[1]), l = atoi(argv[2]);
    if (t >= 0 && t <= 50) // check temperature
    {
      if (l >= 0 && l <= 3) // check luminosity
      {
        alat = t;
        alal = l;
        printf("\nSensor thresholds correctly set!\n");
      }
      else printf("\nInvalid luminosity!\n");
    }
    else printf("\nInvalid temperature!\n");
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xAlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_aa  - activate/deactivate alarms (A/a)
+--------------------------------------------------------------------------*/ 
void cmd_aa (int argc, char** argv) 
{
  xSemaphoreTake(xAlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    uint8_t num = atoi(argv[1]);
    if (num == 65 || num == 97) // ASCII: A = 65, a = 97
    {
      alaf = (num == 65) ? 1 : 0;
      printf("\nAlarm mode correctly set!\n");
    }
    else printf("\nInvalid character!\n");
  }
  else printf("\nInvalid number of arguments!\n");

  xSemaphoreGive(xAlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_cai - clear alarm info (letters CTL in LCD)
+--------------------------------------------------------------------------*/ 
void cmd_cai (int argc, char** argv) 
{
  /*
  xSemaphoreTake(PrintingMutex, portMAX_DELAY);
  // only executed if mutex obtained
  lcd.locate(x,y); // TODO: define cursor position
  LCD.printf("   ");

  xSemaphoreGive(PrintingMutex);
  */
}
/*-------------------------------------------------------------------------+
| Function: cmd_ir  - information about records (NR, nr, wi, ri)
+--------------------------------------------------------------------------*/ 
void cmd_ir (int argc, char** argv) 
{
  xSemaphoreTake(xProcessingMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nNR = %u, nr = %u, wi = %u, ri = %u\n", NR, nr, wi, ri);
  
  xSemaphoreGive(xProcessingMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_lr  - list n records from index i (0 - oldest)
+--------------------------------------------------------------------------*/ 
void cmd_lr (int argc, char** argv) 
{
  if (argc == 3)
  {
    RecordsList data;
    data.n = atoi(argv[1]);
    data.i = atoi(argv[2]);
    if (data.n >= 0 && data.n <= NR) // check n
    {
      if (data.i >= 0 && data.i < NR) // check i
      {
        // TODO: write data in the queue to make it available for processing
        // TODO: read data from the queue and print it to the console
      }
      else printf("\nInvalid number!\n");
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
  // TODO: delete all records from shared memory
  // directly empty the ring buffer or do it through a task?
}
/*-------------------------------------------------------------------------+
| Function: cmd_pr  - process records (max, min, mean) between instants t1 and t2 (h,m,s)
+--------------------------------------------------------------------------*/ 
void cmd_pr (int argc, char** argv) 
{
  Time time1, time2;
  switch (argc)
  {
    case 1:
      // TODO: let TaskProcessing to process all records and read queue to display results
      break;

    case 4:
      time1.hours = atoi(argv[1]); 
      time1.minutes = atoi(argv[2]); 
      time1.seconds = atoi(argv[3]);
      if (checkTime(&time1))
      {
        // TODO: send time1 to the queue and read the queue to display results 
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
          // TODO: send time1, time2 to the queue and read the queue to display results 
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