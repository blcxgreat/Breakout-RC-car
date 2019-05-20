#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/sysctl.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/pin_map.h"
#include "inc/hw_types.h"
#include "driverlib/sysctl.h"
#include "FreeRTOS.h"
#include "sleep.h"

//*****************************************************************************
//
// Implement sleep feature.
//
//*****************************************************************************

void sleep(int n) {  
  for (int i = 0; i < n; i++) {    
    portSUPPRESS_TICKS_AND_SLEEP((uint32_t)0x0001); 
  }
}