// Lab3 partA program

#include <tm4c123gh6pm.h>
#include <stdint.h>
#include "SSD2119.h"
#include "tm4c123gh6pm.h"
#include "stdint.h"
#include <math.h>

//*****************************************************************************

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
#include "sleep.h"

//*****************************************************************************
//
// The stack size for the display task.
//
//*****************************************************************************
#define BREAKTASKSTACKSIZE        128         // Stack size in words

//*****************************************************************************
//
// The item size and queue size for the LED message queue.
//
//*****************************************************************************
#define BREAK_ITEM_SIZE           sizeof(uint8_t)
#define BREAK_QUEUE_SIZE          5

//*****************************************************************************
//
// Default LED toggle delay value. LED toggling frequency is twice this number.
//
//*****************************************************************************
#define BREAK_TOGGLE_DELAY        250

//*****************************************************************************
//
// The queue that holds messages sent to the LED task.
//
//*****************************************************************************
xQueueHandle g_pBreakQueue;

//
// [G, R, B] range is 0 to 0xFFFF per color.
//
static uint32_t g_pui32Colors[3] = { 0x0000, 0x0000, 0x0000 };
static uint8_t g_ui8ColorsIndx;

extern xSemaphoreHandle g_pUARTSemaphore;
extern int keepPlaying;

int prevballx=160, prevbally=200, prevpadx=120, prevpady=225, padx=120, pady=225, ballx=160, bally=200, vx=1, vy=1, temp=0, lives = 3, points = 0, start = 0, blockx = 34, blocky = 10;
int pause = 0;
int count = 0;



struct block {
  int x;
  int y;
  int clear;
  int surprise;
};

struct block arr_block[20];

void PortF_Init(void) {
  SYSCTL_RCGC2_R = SYSCTL_RCGC2_GPIOF;  //enable Port F GPIO
  while((SYSCTL_PRGPIO_R&0x0020) == 0){};/* ready? */
  GPIO_PORTF_DIR_R = 0x0E;  //Set portF as output
  GPIO_PORTF_DEN_R = 0x1F;  //Enable digital port F
  GPIO_PORTF_DATA_R = 0;  //Clear all port F
  GPIO_PORTF_LOCK_R = 0x4C4F434B;   //Unlock the corresponding resister
  GPIO_PORTF_CR_R = 0xFF;   //Un-committing the register
  GPIO_PORTF_PUR_R = 0x11;   //Control one register under GPIOF_CR_R
}

void moveBall(int dx, int dy) {
   ballx += dx;
   bally += dy;
   LCD_DrawFilledCircle(prevballx, prevbally, 8, BLACK);
   LCD_DrawFilledCircle(ballx, bally, 8, WHITE);
}

void movePaddleLeft() {
   padx -= 5;
   if(padx <= 0){
      padx = 0;
      LCD_DrawFilledRect(0, pady, 60, 12, WHITE);
   } else {
      LCD_DrawFilledRect(prevpadx + 55, prevpady, 5, 15, BLACK);
      LCD_DrawFilledRect(padx, pady, 60, 12, WHITE);
   }
}

void movePaddleRight() {
   padx += 5;
   if (padx >= 240){
      padx = 240;
      LCD_DrawFilledRect(240, pady, 60, 12, WHITE);
   } else {
      LCD_DrawFilledRect(prevpadx, prevpady, 5, 15, BLACK);
      LCD_DrawFilledRect(padx, pady, 60, 12, WHITE);
   }
}

//Function Definitions
int BounceRight(void) {
  if(ballx + 8 >= 320) {
    return 1;
  } else {
    return 0;
  }
}
 
int BounceLeft(void) {
  if(ballx - 8 <= 0){
    return 1;
  }else {
    return 0; 
  }
}

int BounceTop(void) {
  if(bally - 8 <= 0) {
    return 1;
  } else {
    return 0;
  }
}

int BouncePaddle(void) {
  if((bally + 8 >= 224)) {
    if((ballx + 8 - padx > 0) && (ballx + 8 - padx < 88)) {
      return 1;  //Bounced Off Paddle
    } else {
      return 2; //Missed Paddle Life Lost
    }
  } else {
    return 0;  //Far Away From Paddle
  }
}

int BounceBlock(void) {
  for(int i = 0; i < 20; i++) {
    if(arr_block[i].clear == 0) {
      if(abs(ballx - arr_block[i].x) < 37){
        if(abs(bally - arr_block[i].y) < 13){
          return i+1;
        }
      }
      
    }
  }
  return 0;
}

void drawBlocks(void){
  for(int i = 0; i < 20; i++){
    arr_block[i].x = blockx;
    arr_block[i].y = blocky;
    arr_block[i].clear = 0;
    if(blockx == 286){
      blockx = 34;
      blocky += 15;
    } else {
      blockx += 63;
    }
  }
  
  for(int i = 0; i < 20; i++){
    if(i <= 4){
      LCD_DrawFilledRect(arr_block[i].x - 29, arr_block[i].y - 5, 58, 10, LIGHTBLUE);
    } else if(i <= 9){
      LCD_DrawFilledRect(arr_block[i].x - 29, arr_block[i].y - 5, 58, 10, RED);
    } else if(i <= 14){
      LCD_DrawFilledRect(arr_block[i].x - 29, arr_block[i].y - 5, 58, 10, GREEN);
    } else {
      LCD_DrawFilledRect(arr_block[i].x - 29, arr_block[i].y - 5, 58, 10, PINK);
    }
  }
}

unsigned long SwitchTo(void) {
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
  
  if(typez1 > (unsigned long)1000 && ((abs(typex - 240) <= 40) && (abs(typey - 140) <= 40) )){
    return 1;
  } else {
    return 0;
  }
}

static void
BreakoutTask(void *pvParameters)
{
  
  PortF_Init();
  LCD_Init(); 
  Touch_Init();
  LCD_DrawFilledCircle(prevballx, prevbally, 8, WHITE);
  LCD_DrawFilledRect(prevpadx, prevpady, 60, 10, WHITE);
  drawBlocks();
  
  while (1) {
    
        vTaskDelay(5 / portTICK_RATE_MS);
    
    // move padle to start the game
    if(GPIO_PORTF_DATA_R == 0x01){ // paddle left
      start = 1;
      if(count == 0){
        /* random int between 0 and 19 */
        int r = rand() % 20;
        arr_block[r].surprise = 1;
        for(int i = 0; i < 20; i++){
          if(i != r){
            arr_block[i].surprise = 0;
          }
        }
      }
      movePaddleLeft();
    } else if(GPIO_PORTF_DATA_R == 0x10){ // paddle right
      start = 1;
      if(count == 0){
        /* random int between 0 and 19 */
        int r = rand() % 20;
        arr_block[r].surprise = 1;
        for(int i = 0; i < 20; i++){
          if(i != r){
            arr_block[i].surprise = 0;
          }
        }
      }
      movePaddleRight();
    }
    
    if(start == 1 && pause == 0){
      count++;
    //move ball if start ==1 
      moveBall(vx, vy);
 
      //check for bouncing off three sides
      //change vx and vy
      if(BounceRight() || BounceLeft()) {
        vx = -vx;
      }
      if(BounceTop()) {
        vy = -vy;
      }
    
      //check for bouncing off paddle
      //change vx, vy or reduce lives
      switch(BouncePaddle())
      {
        case(0):
        break;
        case(1): 
          vy= -vy;
        break;
        case(2): 
          start = 0;
        break;
      }
      
      // check for bouncing off blocks
      temp = BounceBlock();
      if(temp) {
        points++;
        //set black (todo)
        arr_block[temp-1].clear = 1;
        LCD_DrawFilledRect(arr_block[temp-1].x - 29, arr_block[temp-1].y - 5, 58, 10, BLACK);
        if(arr_block[temp-1].surprise == 1){
          pause = 1;
        }
        vy = -vy;
        vx = -vx;
      }
      
      prevballx = ballx;
      prevbally = bally;
      prevpadx = padx;
    } else if(pause == 1){
      count++;
      LCD_DrawFilledCircle(160, 140, 40, ORANGE);
      LCD_DrawFilledCircle(160, 140, 30, YELLOW);
      if(SwitchTo()){
        LCD_DrawFilledCircle(160, 140, 40, BLACK);
        keepPlaying = 0;
        vTaskDelay(5 / portTICK_RATE_MS);
      } else {
         vTaskDelay(INFINITY / portTICK_RATE_MS);
//        if(SwitchTo() == 0){
//          pause = 0;
//          LCD_DrawFilledCircle(160, 140, 40, BLACK);
//        }
       
        
      }
    
    } else {
      count = 0;
      for(int i = 0; i < 20; i++){
        arr_block[i].surprise = 0;
      }
      LCD_DrawFilledRect(0, 0, 320, 240, BLACK);
      blockx = 34;
      blocky = 10;
      drawBlocks();
      LCD_DrawFilledCircle(ballx, bally, 8, BLACK);
      LCD_DrawFilledRect(padx, pady, 60, 15, BLACK);
      prevballx=160;
      prevbally=200;
      prevpadx=120;
      prevpady=225;
      padx=120; 
      pady=225;
      ballx=160;
      bally=200;
      vx=4;
      vy=4;
      LCD_DrawFilledCircle(prevballx, prevbally, 8, WHITE);
      LCD_DrawFilledRect(prevpadx, prevpady, 60, 10, WHITE);
    }
  }
}

uint32_t BreakoutTaskInit(void)
{   
    // create a queue for sending the tasks
    g_pBreakQueue = xQueueCreate(BREAK_QUEUE_SIZE, BREAK_ITEM_SIZE);
    
    
    if(xTaskCreate(BreakoutTask, (const portCHAR *)"Breakout",
                   BREAKTASKSTACKSIZE, NULL,  1, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}


