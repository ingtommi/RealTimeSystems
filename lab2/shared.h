#include <cstdint>

#ifndef SHARED_H
#define SHARED_H

#define NR 20 // maximum size of the buffer
#define INVALID -1

// short data type used to avoid misdetection of overflows finishing in the correct range
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

typedef struct
{
  uint8_t temp;
  uint8_t lum;
} Sensor;

typedef struct
{
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
  uint8_t temperature;
  uint8_t luminosity;
} Record;

typedef struct
{
  uint8_t maxT;
  uint8_t minT;
  float meanT;
  uint8_t maxL;
  uint8_t minL;
  float meanL;
} Process;

#endif /* SHARED_H */