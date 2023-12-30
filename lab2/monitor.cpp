#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*-------------------------------------------------------------------------+
| Headers of command functions
+--------------------------------------------------------------------------*/ 
       void cmd_sos (int, char**);
extern void cmd_send (int, char**);
extern void cmd_rc (int, char**);
extern void cmd_sc (int, char**);
extern void cmd_rtl (int, char**);
extern void cmd_rp (int, char**);
extern void cmd_mmp (int, char**);
extern void cmd_mta (int, char**);
extern void cmd_mpp (int, char**);
extern void cmd_rai (int, char**);
extern void cmd_dac (int, char**);
extern void cmd_dtl (int, char**);
extern void cmd_aa (int, char**);
extern void cmd_cai (int, char**);
extern void cmd_ir (int, char**);
extern void cmd_lr (int, char**);
extern void cmd_dr (int, char**);
extern void cmd_pr (int, char**);

/*-------------------------------------------------------------------------+
| Variable and constants definition
+--------------------------------------------------------------------------*/ 
const char TitleMsg[] = "Application Control Monitor\n";
const char InvalMsg[] = "\nInvalid command!\n";
const char DescrMsg[] = "\nCMD ARGUMENTS                 DESCRIPTION\n";

struct  command_d {
  void  (*cmd_fnct)(int, char**);
  char* cmd_name;
  char* cmd_help;
} const commands[] = {
  {cmd_sos, "sos", "                          - display commands"},
  {cmd_rc,  "rc",  "                           - read clock"},
  {cmd_sc,  "sc",  " hh mm ss                  - set clock"},
  {cmd_rtl, "rtl", "                          - read temperature and luminosity"},
  {cmd_rp,  "rp",  "                           - read parameters (pmon, tala, pproc)"},
  {cmd_mmp, "mmp", "p                         - modify monitoring period (seconds - 0 deactivate)"},
  {cmd_mta, "mta", "t                         - modify time alarm (seconds)"},
  {cmd_mpp, "mpp", "p                         - modify processing period (seconds - 0 deactivate)"},
  {cmd_rai, "rai", "                          - read alarm info (clock, temperature, luminosity, active/inactive-A/a)"},
  {cmd_dac, "dac", "hh mm ss                  - define alarm clock"},
  {cmd_dtl, "dtl", "T L                       - define alarm temperature and luminosity"},
  {cmd_aa,  "aa",  " a                         - activate/deactivate alarms (A/a)"},
  {cmd_cai, "cai", "                          - clear alarm info (letters CTL in LCD)"},
  {cmd_ir,  "ir",  "                           - information about records (NR, nr, wi, ri)"},
  {cmd_lr,  "lr",  " n i                       - list n records from index i (0 - oldest)"},
  {cmd_dr,  "dr",  "                           - delete records"},
  {cmd_pr,  "pr",  " hh1 mm1 ss1 hh2 mm2 ss2   - process records (max, min, mean) between instants t1 and t2 (h,m,s)"}
};

#define NCOMMANDS  (sizeof(commands)/sizeof(struct command_d))
#define ARGVECSIZE 3
#define MAX_LINE   50

/*-------------------------------------------------------------------------+
| Function: cmd_sos - provides a rudimentary help
+--------------------------------------------------------------------------*/ 
void cmd_sos (int argc, char **argv)
{
  int i;

  printf("%s\n", DescrMsg);
  for (i = 0; i < NCOMMANDS; i++)
    printf("%s %s\n", commands[i].cmd_name, commands[i].cmd_help);
}

/*-------------------------------------------------------------------------+
| Function: my_fgets        (called from my_getline) 
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

/*-------------------------------------------------------------------------+
| Function: my_getline        (called from monitor) 
+--------------------------------------------------------------------------*/ 
int my_getline (char** argv, int argvsize)
{
  static char line[MAX_LINE];
  char *p;
  int argc;

  //fgets(line, MAX_LINE, stdin);
  my_fgets(line, MAX_LINE, stdin);

  /* Break command line into an o.s. like argument vector,
     i.e. compliant with the (int argc, char **argv) specification -------- */

  for (argc = 0, p = line; (*line != '\0') && (argc < argvsize); p = NULL, argc++) {
    p = strtok(p, " \t\n");
    argv[argc] = p;
    if (p == NULL) return argc;
  }
  argv[argc] = p;
  return argc;
}

/*-------------------------------------------------------------------------+
| Function: monitor        (called from main) 
+--------------------------------------------------------------------------*/ 
void monitor (void)
{
  static char *argv[ARGVECSIZE+1], *p;
  int argc, i;

  printf("%s\nType sos for help\n", TitleMsg);
  for (;;) {
    printf("\nCMD> ");
    /* Reading and parsing command line  ----------------------------------*/
    if ((argc = my_getline(argv, ARGVECSIZE)) > 0) 
    {
      for (p = argv[0]; *p != '\0'; *p = tolower(*p), p++);
      for (i = 0; i < NCOMMANDS; i++) 
      if (strcmp(argv[0], commands[i].cmd_name) == 0) 
        break;
      /* Executing commands -----------------------------------------------*/
      if (i < NCOMMANDS)
        commands[i].cmd_fnct(argc, argv);
      else  
        printf("%s", InvalMsg);
    }
  } // forever
}