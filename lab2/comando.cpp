#include <stdio.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

extern QueueHandle_t xQueue;

/*-------------------------------------------------------------------------+
| Function: cmd_rc  - read clock
+--------------------------------------------------------------------------*/ 
void cmd_rc (int argc, char** argv) 
{
  //printf("%d\n", xTaskGetTickCount()/configTICK_RATE_HZ);
}
/*-------------------------------------------------------------------------+
| Function: cmd_sc  - set clock
+--------------------------------------------------------------------------*/ 
void cmd_sc (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_rtl - read temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_rtl (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_rp  - read parameters (pmon, tala, pproc)
+--------------------------------------------------------------------------*/ 
void cmd_rp (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_mmp - modify monitoring period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mmp (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_mta - modify time alarm (seconds)
+--------------------------------------------------------------------------*/ 
void cmd_mta (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_mpp - modify processing period (seconds - 0 deactivate)
+--------------------------------------------------------------------------*/ 
void cmd_mpp (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_rai - read alarm info (clock, temperature, luminosity, active/inactive-A/a)
+--------------------------------------------------------------------------*/ 
void cmd_rai (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_dac - define alarm clock
+--------------------------------------------------------------------------*/ 
void cmd_dac (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_dtl - define alarm temperature and luminosity
+--------------------------------------------------------------------------*/ 
void cmd_dtl (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_aa  - activate/deactivate alarms (A/a)
+--------------------------------------------------------------------------*/ 
void cmd_aa (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_cai - clear alarm info (letters CTL in LCD)
+--------------------------------------------------------------------------*/ 
void cmd_cai (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_ir  - information about records (NR, nr, wi, ri)
+--------------------------------------------------------------------------*/ 
void cmd_ir (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_lr  - list n records from index i (0 - oldest)
+--------------------------------------------------------------------------*/ 
void cmd_lr (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_dr  - delete records
+--------------------------------------------------------------------------*/ 
void cmd_dr (int argc, char** argv) 
{

}
/*-------------------------------------------------------------------------+
| Function: cmd_pr  - process records (max, min, mean) between instants t1 and t2 (h,m,s)
+--------------------------------------------------------------------------*/ 
void cmd_pr (int argc, char** argv) 
{

}