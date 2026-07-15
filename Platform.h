#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <stdint.h>
#include "RegisterDefine.h"

typedef __bit PlatformIrqState;

#define MEM_XDATA(type)     type __xdata
#define MEM_CODE(type)      type __code
#define MEM_FAST_DATA(type) type __data

#define Platform_IrqSave(state) \
	do { (state) = EA; EA = 0; } while (0)

#define Platform_IrqRestore(state) \
	do { EA = (state); } while (0)

#define Platform_IrqEnableGlobal() \
	do { EA = 1; } while (0)

#endif
