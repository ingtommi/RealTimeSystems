#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "semphr.h"

#define NR 5 // redefined here to avoid using an header file

//extern QueueHandle_t xQueue;

extern SemaphoreHandle_t ClockMutex, ParamMutex, AlarmMutex, ProcessingMutex;

extern std::uint8_t hours, minutes, seconds;
extern std::uint8_t pmon, tala, pproc; 
extern std::uint8_t alah, alam, alas, alat, alal;
extern bool alaf; // alaf = 0 --> a, alaf = 1 --> A
extern std::uint8_t nr, wi, ri, n, i;

/*-------------------------------------------------------------------------+
| Function: cmd_rc  - read clock
+--------------------------------------------------------------------------*/ 
void cmd_rc (int argc, char** argv) 
{
  xSemaphoreTake(ClockMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nCurrent clock: %02d:%02d:%02d", hours, minutes, seconds);

  xSemaphoreGive(ClockMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_sc  - set clock
+--------------------------------------------------------------------------*/ 
void cmd_sc (int argc, char** argv) 
{
  xSemaphoreTake(ClockMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 4)
  {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) < 24) // check hours
    {
      if (atoi(argv[2]) >= 0 && atoi(argv[2]) < 60) // check minutes
      {
        if (atoi(argv[3]) >= 0 && atoi(argv[3]) < 60) // check seconds
        {
          hours = atoi(argv[1]);
          minutes = atoi(argv[2]);
          seconds = atoi(argv[3]);
          printf("\nClock correctly set!\n");
        }
      }
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(ClockMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_rtl - read temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_rtl (int argc, char** argv) 
{
  // Ask TaskSensor to read sensors and print current T, L on the console
}
/*-------------------------------------------------------------------------+
| Function: cmd_rp  - read parameters (pmon, tala, pproc)
+--------------------------------------------------------------------------*/ 
void cmd_rp (int argc, char** argv) 
{
  xSemaphoreTake(ParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nPMON = %u, TALA = %u, PPROC = %u seconds\n", pmon, tala, pproc);

  xSemaphoreGive(ParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mmp - modify monitoring period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mmp (int argc, char** argv) 
{
  xSemaphoreTake(ParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) < 60) // check seconds
    {
      pmon = atoi(argv[1]);
      printf("\nMonitoring period correctly set!\n");
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(ParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mta - modify time alarm (seconds)
+--------------------------------------------------------------------------*/ 
void cmd_mta (int argc, char** argv) 
{
  xSemaphoreTake(ParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) < 60) // check seconds
    {
      tala = atoi(argv[1]);
      printf("\nAlarm time correctly set!\n");
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(ParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_mpp - modify processing period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mpp (int argc, char** argv) 
{
  xSemaphoreTake(ParamMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) < 60) // check seconds
    {
      pproc = atoi(argv[1]);
      printf("\nProcessing period correctly set!\n");
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(ParamMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_rai - read alarm info (clock, temperature, luminosity, active/inactive-A/a)
+--------------------------------------------------------------------------*/ 
void cmd_rai (int argc, char** argv) 
{
  xSemaphoreTake(AlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nALAH = %u, ALAM = %u, ALAS = %u\n", alah, alam, alas);
  if (alaf) 
    printf("ALAT = %u, ALAL = %u, ALAF = A\n", alat, alal);
  else 
    printf("ALAT = %u, ALAL = %u, ALAF = a\n", alat, alal); 

  xSemaphoreGive(AlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_dac - define alarm clock
+--------------------------------------------------------------------------*/ 
void cmd_dac (int argc, char** argv) 
{
  xSemaphoreTake(AlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 4)
  {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) < 24) // check hours
    {
      if (atoi(argv[2]) >= 0 && atoi(argv[2]) < 60) // check minutes
      {
        if (atoi(argv[3]) >= 0 && atoi(argv[3]) < 60) // check seconds
        {
          alah = atoi(argv[1]);
          alam = atoi(argv[2]);
          alas = atoi(argv[3]);
          printf("\nClock thresholds correctly set!\n");
        }
      }
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(AlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_dtl - define alarm temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_dtl (int argc, char** argv) 
{
  xSemaphoreTake(AlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 3)
  {
    if (atoi(argv[1]) >= 0 && atoi(argv[1]) <= 50) // check temperature
    {
      if (atoi(argv[2]) >= 0 && atoi(argv[2]) <= 3) // check luminosity
      {
        alat = atoi(argv[1]);
        alal = atoi(argv[2]);
        printf("\nSensor thresholds correctly set!\n");
      }
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(AlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_aa  - activate/deactivate alarms (A/a)
+--------------------------------------------------------------------------*/ 
void cmd_aa (int argc, char** argv) 
{
  xSemaphoreTake(AlarmMutex, portMAX_DELAY);
  // only executed if mutex obtained
  if (argc == 2)
  {
    if (atoi(argv[1]) == 65 || atoi(argv[1]) == 97) // ASCII: A = 65, a = 97
    {
      if (atoi(argv[1]) == 65) alaf = 1;
      else alaf = 0;
      printf("\nAlarm mode correctly set!\n");
    }
  }
  else printf("\nInvalid number of arguments or wrong arguments!\n");

  xSemaphoreGive(AlarmMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_cai - clear alarm info (letters CTL in LCD)
+--------------------------------------------------------------------------*/ 
void cmd_cai (int argc, char** argv) 
{
  // Clear letters
}
/*-------------------------------------------------------------------------+
| Function: cmd_ir  - information about records (NR, nr, wi, ri)
+--------------------------------------------------------------------------*/ 
void cmd_ir (int argc, char** argv) 
{
  xSemaphoreTake(ProcessingMutex, portMAX_DELAY);
  // only executed if mutex obtained
  printf("\nNR = %u, nr = %u, wi = %u, ri = %u\n", NR, nr, wi, ri);
  
  xSemaphoreGive(ProcessingMutex);
}
/*-------------------------------------------------------------------------+
| Function: cmd_lr  - list n records from index i (0 - oldest)
+--------------------------------------------------------------------------*/ 
void cmd_lr (int argc, char** argv) 
{
  // Accept n, i, read from memory and print records on the console
}
/*-------------------------------------------------------------------------+
| Function: cmd_dr  - delete records
+--------------------------------------------------------------------------*/ 
void cmd_dr (int argc, char** argv) 
{
  // Delete all records from shared memory
}
/*-------------------------------------------------------------------------+
| Function: cmd_pr  - process records (max, min, mean) between instants t1 and t2 (h,m,s)
+--------------------------------------------------------------------------*/ 
void cmd_pr (int argc, char** argv) 
{
  // Accept t1, t2 and process records
}