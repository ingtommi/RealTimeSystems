#include <cstdint>

#ifndef SHARED_H
#define SHARED_H

#define NR 20 // maximum size of the buffer
#define INVALID -1

// Used for tasks receiving data from multiple sources
typedef enum
{
  TIMER,
  CONSOLE
} Sender;

typedef struct 
{
  short hours;
  short minutes;
  short seconds;
} Time;

typedef struct 
{
  Time time1;
  Time time2;
} Interval;

// Console/Timer -> Processing
typedef struct
{
  Interval interval;
  Sender sender;
} InputData;

// Sensors -> Console
typedef struct
{
  uint8_t temp;
  uint8_t lum;
} Sensor;

// Sensors -> memory, memory -> Processing
typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t temperature;
  uint8_t luminosity;
} Record;

// Processing -> Console
typedef struct
{
  uint8_t maxT;
  uint8_t minT;
  float meanT;
  uint8_t maxL;
  uint8_t minL;
  float meanL;
} OutputData;

#endif /* SHARED_H */
