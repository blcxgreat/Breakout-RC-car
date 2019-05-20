//*****************************************************************************
//
// led_task.c - A simple flashing LED task.
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

#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/gpio.h"
#include "driverlib/rom.h"
#include "drivers/rgb.h"
#include "drivers/buttons.h"
#include "utils/uartstdio.h"
#include "led_task.h"
#include "priorities.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "tm4c123gh6pm.h"


//*****************************************************************************
//
// The stack size for the LED toggle task.
//
//*****************************************************************************
#define LEDTASKSTACKSIZE        128         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the LED message queue.
//
//*****************************************************************************
#define LED_ITEM_SIZE           sizeof(uint8_t)
#define LED_QUEUE_SIZE          5

//*****************************************************************************
//
// Default LED toggle delay value. LED toggling frequency is twice this number.
//
//*****************************************************************************
#define LED_TOGGLE_DELAY        250

//*****************************************************************************
//
// The queue that holds messages sent to the LED task.
//
//*****************************************************************************
xQueueHandle g_pLEDQueue;

//
// [G, R, B] range is 0 to 0xFFFF per color.
//
static uint32_t g_pui32Colors[3] = { 0x0000, 0x0000, 0x0000 };
static uint8_t g_ui8ColorsIndx;

extern xSemaphoreHandle g_pUARTSemaphore;

//*****************************************************************************
//
// This task toggles the user selected LED at a user selected frequency. User
// can make the selections by pressing the left and right buttons.
//
//*****************************************************************************

void LED_Init(void) { volatile unsigned long delay;
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE;       // Activate clock for port A
  delay = SYSCTL_RCGC2_R;             // Allow time for clock to initialize
  GPIO_PORTE_PCTL_R &= ~0x0000FFF0;   // Regular GPIO
  GPIO_PORTE_AMSEL_R &= ~0x0E;        // Disable analog on PA2
  GPIO_PORTE_DIR_R |= 0x0E;          // Direction PA2 output
  GPIO_PORTE_AFSEL_R &= ~0x0E;        // regular port function
  GPIO_PORTE_DEN_R |= 0x0E;           // Enable digital port
}

// LED On/Off functions
void LEDG_On(void) {
  GPIO_PORTE_DATA_R |= 0x08;
}
void LEDG_Off(void) {
  GPIO_PORTE_DATA_R &= ~0x08;
}
void LEDY_On(void) {
  GPIO_PORTE_DATA_R |= 0x04;
}
void LEDY_Off(void) {
  GPIO_PORTE_DATA_R &= ~0x04;
}
void LEDR_On(void) {
  GPIO_PORTE_DATA_R |= 0x02;
}
void LEDR_Off(void) {
  GPIO_PORTE_DATA_R &= ~0x02;
}

// Set state functions
void All_Off(void) {
  LEDG_Off();
  LEDY_Off();
  LEDR_Off();
}
void REDled (void) {
  LEDG_Off();
  LEDY_Off();
  LEDR_On();
}
void GREENled (void) {
  LEDG_On();
  LEDY_Off();
  LEDR_Off();
}
void YELLOWled (void) {
  LEDG_Off();
  LEDY_On();
  LEDR_Off();
}


static void
LEDTask(void *pvParameters)
{
    portTickType ui32WakeTime;
    uint32_t ui32LEDToggleDelay;
    uint8_t i8Message;
    int ps = 0; // 0:stop, 1:RED, 2:GREEN; 3:YELLOW
    int ns = 0; // 0:stop, 1:RED, 2:GREEN; 3:YELLOW
    int ledColor = 0;
    int changed = 0;
    
    //
    // Initialize the LED Toggle Delay to default value.
    //
    ui32LEDToggleDelay = LED_TOGGLE_DELAY;

    //
    // Get the current tick count.
    //
    ui32WakeTime = xTaskGetTickCount();

    //
    // Loop forever.
    //
    while(1)
    {
        //
        // Read the next message, if available on queue.
        //
      if(changed == 0){
        if(xQueueReceive(g_pLEDQueue, &i8Message, 0) == pdPASS)
        {
          switch(ps){
            case(0): // off
              if(i8Message == LEFT_BUTTON){
                ledColor = 1;
                ns = 1;
              } else {
                ledColor = 0;
                ns = 0;
              }
            break;
            
            case(1): // red
              if(i8Message == LEFT_BUTTON){
                ledColor = 0;
                ns = 0;
              } else {
                ledColor = 2;
                ns = 2;
              }
            break;
            
            case(2): // green
              if(i8Message == LEFT_BUTTON){
                ledColor = 0;
                ns = 0;
              } else if(i8Message == RIGHT_BUTTON) {
                ledColor = 3;
                ns = 3;
              } else {
                ledColor = 1;
                ns = 1;
              }
            break;
            
            case(3): // yellow
              if(i8Message == LEFT_BUTTON){
                ledColor = 0;
                ns = 0;
              } else {
                ledColor = 1;
                ns = 1;
              }
            break;
              
          }

        } else {
          if(ps == 1){
            ns = 2;
          } else if(ps == 0){
            ns = 0;
          } else {
            ns = 1;
          }
        }
      } 
      
        ps = ns;
        //
        // Turn on the LED.
        //
        if(ps == 0){
          All_Off();
        } else if(ps == 1){
          REDled();
        } else if(ps == 2){
          GREENled();
        } else if(ps == 3){
          YELLOWled();
          //break;
        }

        //
        // Wait for the required amount of time.
        //
        for(int i = 0; i<50; i++){
          vTaskDelayUntil(&ui32WakeTime, 100/*ui32LEDToggleDelay*/ / portTICK_RATE_MS);
          if(xQueueReceive(g_pLEDQueue, &i8Message, 0) == pdPASS){
            if((ps == 0) && (i8Message == LEFT_BUTTON)){
              ns = 1;
              changed = 1;
              break;
            } else if((ps == 2) && (i8Message == RIGHT_BUTTON)){
              ns = 3;
              changed = 1;
              break;
            } else if((ps != 0) && (i8Message == LEFT_BUTTON)){
              ns = 0;
              changed = 1;
              break;
            }
          }
          changed = 0;
       }
    }
}

//*****************************************************************************
//
// Initializes the LED task.
//
//*****************************************************************************
uint32_t
LEDTaskInit(void)
{   
  
    LED_Init();
    //
    // Initialize the GPIOs and Timers that drive the three LEDs.
    //
    RGBInit(1);
    RGBIntensitySet(0.3f);

    //
    // Turn on the Green LED
    //
    g_ui8ColorsIndx = 0;
    g_pui32Colors[g_ui8ColorsIndx] = 0x8000;
    RGBColorSet(g_pui32Colors);

    //
    // Print the current loggling LED and frequency.
    //
    UARTprintf("\nLed %d is blinking. [R, G, B]\n", g_ui8ColorsIndx);
    UARTprintf("Led blinking frequency is %d ms.\n", (LED_TOGGLE_DELAY * 2));

    //
    // Create a queue for sending messages to the LED task.
    //
    g_pLEDQueue = xQueueCreate(LED_QUEUE_SIZE, LED_ITEM_SIZE);

    //
    // Create the LED task.
    //
    if(xTaskCreate(LEDTask, (const portCHAR *)"LED", LEDTASKSTACKSIZE, NULL,
                   tskIDLE_PRIORITY + PRIORITY_LED_TASK, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}
