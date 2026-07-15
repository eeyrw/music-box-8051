#ifndef __NONLINEAR_MAP_TABLE_H__
#define __NONLINEAR_MAP_TABLE_H__

#include <stdint.h>
#include "Platform.h"

#define NONLINEAR_MAP_POWER2  0
#define NONLINEAR_MAP_POWER06 1
#define NONLINEAR_MAP_DB20    2

#define NONLINEAR_MAP_CURVE NONLINEAR_MAP_DB20

extern MEM_CODE(const uint8_t) NonlinearMapTable[128];

#endif
