//*****************************************************************************
//
// switch_task.c - A simple switch task to process the buttons.
//
// Copyright (c) 2012-2017 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 2.1.4.178 of the EK-TM4C123GXL Firmware Package.
//
//*****************************************************************************

#include "SSD2119.h"
#include "sleep.h"
#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "switch_task.h"
#include "led_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

//*****************************************************************************
//
// The stack size for the display task.
//
//*****************************************************************************
#define SWITCHTASKSTACKSIZE        128         // Stack size in words

extern xQueueHandle g_pLEDQueue;
extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task reads the buttons' state and passes this information to LEDTask.
//
//*****************************************************************************


// lcd check function
unsigned long Switch1(void) {
  int typex = Touch_ReadX();
 int typey = Touch_ReadY();
  unsigned long typez1 = Touch_ReadZ1();
  typex = typex/3 - 450;
  typey = typey/5 - 250;
  if(typex < 0){
    typex = 0;
  } else if(typex > 320){
    typex = 320;
  }
  
  if(typey < 0){
    typey = 0;
  } else if(typey > 240){
    typey = 240;
  }
  
  if(typez1 > (unsigned long)1000 && (typex <= 50 && typey >= 200)){
    return 1;
  } else {
    return 0;
  }
}

unsigned long Switch2(void) {
  int typex = Touch_ReadX();
  int typey = Touch_ReadY();
  unsigned long typez1 = Touch_ReadZ1();
  typex = typex/3 - 450;
  typey = typey/5 - 250;
  if(typex < 0){
    typex = 0;
  } else if(typex > 320){
    typex = 320;
  }
  
  if(typey < 0){
    typey = 0;
  } else if(typey > 240){
    typey = 240;
  }
  
  if(typez1 > (unsigned long)1000 && (typex <= 120 && typey <= 90)){
    return 1;
  } else {
    return 0;
  }
}

static uint8_t buttonState (void){
  if(Switch1() || Switch2()){
    return 1;
  } else {
    return 0;
  }
}

static void
SwitchTask(void *pvParameters)
{
    LCD_Init();
    Touch_Init();
    portTickType ui16LastTime;
    uint32_t ui32SwitchDelay = 25;
    uint8_t ui8CurButtonState, ui8PrevButtonState;
    uint8_t ui8Message;

    ui8CurButtonState = ui8PrevButtonState = 0;

    //
    // Get the current tick count.
    //
    ui16LastTime = xTaskGetTickCount();
 
    LCD_DrawFilledRect(265, 0, 55, 55, GREEN); // under face (rect)
    LCD_DrawFilledRect(265, 185, 55, 55, RED); // under face (rect)
    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Poll the debounced state of the buttons.
        //
        ui8CurButtonState = buttonState();

        //
        // Check if previous debounced state is equal to the current state.
        //
        if(ui8CurButtonState != ui8PrevButtonState)
        {
            ui8PrevButtonState = ui8CurButtonState;
            
            vTaskDelayUntil(&ui16LastTime, 2000 / portTICK_RATE_MS);
            //
            // Check to make sure the change in state is due to button press
            // and not due to button release.
            //
            if(Switch1() || Switch2())
            {
                if(Switch1())
                {
                    ui8Message = LEFT_BUTTON;
                    //sleep(3000); // here we call sleep task
                    LCD_DrawFilledCircle(160, 120, 55, YELLOW);
                    for(int i = 0; i < 55; i++){
                      LCD_DrawLine(160, 120, 215, 120 - i, BLACK);
                      LCD_DrawLine(160, 120, 215, 120 + i, BLACK);
                    }
                    LCD_DrawFilledRect(125, 205, 80, 10, BLACK);
                    LCD_SetTextColor(255,255,240);
                    LCD_SetCursor(105,195);
                    printf("ACTIVATED / DEACTIVATED");
                    //
                    // Guard UART from concurrent access.
                    //
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Left Button is pressed.\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }
                else if(Switch2())
                {
                    ui8Message = RIGHT_BUTTON;
                    LCD_DrawFilledCircle(160, 120, 55, YELLOW);
                    LCD_DrawFilledRect(105, 195, 150, 10, BLACK);
                    LCD_SetTextColor(255,255,240);
                    LCD_SetCursor(125,205);
                    printf("TURNED YELLOW");
                    //
                    // Guard UART from concurrent access.
                    //
                    xSemaphoreTake(g_pUARTSemaphore, portMAX_DELAY);
                    UARTprintf("Right Button is pressed.\n");
                    xSemaphoreGive(g_pUARTSemaphore);
                }

                //
                // Pass the value of the button pressed to LEDTask.
                //
                if(xQueueSend(g_pLEDQueue, &ui8Message, portMAX_DELAY) !=
                   pdPASS)
                {
                    //
                    // Error. The queue should never be full. If so print the
                    // error message on UART and wait for ever.
                    //
                    UARTprintf("\nQueue full. This should never happen.\n");
                    while(1)
                    {
                    }
                }
            }
        }

        //
        // Wait for the required amount of time to check back.
        //
        vTaskDelayUntil(&ui16LastTime, ui32SwitchDelay / portTICK_RATE_MS);
    }
}

//*****************************************************************************
//
// Initializes the switch task.
//
//*****************************************************************************
uint32_t
SwitchTaskInit(void)
{
    //
    // Unlock the GPIO LOCK register for Right button to work.
    //
    HWREG(GPIO_PORTF_BASE + GPIO_O_LOCK) = GPIO_LOCK_KEY;
    HWREG(GPIO_PORTF_BASE + GPIO_O_CR) = 0xFF;

    //
    // Initialize the buttons
    //
    ButtonsInit();

    //
    // Create the switch task.
    //
    if(xTaskCreate(SwitchTask, (const portCHAR *)"Switch",
                   SWITCHTASKSTACKSIZE, NULL, tskIDLE_PRIORITY +
                   PRIORITY_SWITCH_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}
