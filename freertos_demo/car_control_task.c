#include "tm4c123gh6pm.h"
#include "stdint.h"

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
#define CARTASKSTACKSIZE        128         // Stack size in words

extern xQueueHandle g_pBreakQueue;
extern xSemaphoreHandle g_pUARTSemaphore;
extern int keepPlaying;

void car_init(void);
void forward ();
void backward ();
void turn_right ();
void turn_left ();
void stop();
void UART4_16MHz_Init();
char UART4_readChar(void);
void UART4_printChar(char c);
void UART4_printString(char* string);
uint32_t CarControlTaskInit(void); 

static void
CarControlTask(void *pvParameters)
{

  car_init();
  GPIO_PORTF_DEN_R |= 0xE;             // enable digital port 
  GPIO_PORTF_DIR_R |= 0xE;             // set direction on output
  GPIO_PORTF_DATA_R = 0x0;
  
  UART4_16MHz_Init();
  
  while(1){
    if(keepPlaying == 0){
      car_init();
      stop();
      char c;  
      c = UART4_readChar();  
        
      if (c == 'u'){
        GPIO_PORTF_DATA_R = 0x2;
        forward();
        vTaskDelay(1000 / portTICK_RATE_MS);
        stop();
        GPIO_PORTF_DATA_R &= ~0x2;
      } else if (c == 'd'){
        GPIO_PORTF_DATA_R = 0x4;
        backward();
        vTaskDelay(1000 / portTICK_RATE_MS);
        stop();
        GPIO_PORTF_DATA_R &= ~0x4;
      } else if (c == 'l'){
        GPIO_PORTF_DATA_R = 0x6;
        turn_left();
        vTaskDelay(1000 / portTICK_RATE_MS);
        stop();
        GPIO_PORTF_DATA_R &= ~0x6;
      } else if (c == 'r'){
        GPIO_PORTF_DATA_R = 0x8;
        turn_right();
        vTaskDelay(1000 / portTICK_RATE_MS);
        stop();
        GPIO_PORTF_DATA_R &= ~0x8;
      } else if (c == ' '){
        GPIO_PORTF_DATA_R = 0xE;
      } else {
      //  GPIO_PORTF_DATA_R = 0xA;
      }
      
    }
  }
}

void forward (){
  GPIO_PORTE_DATA_R = 0x0A; // 0010 1010   *
  GPIO_PORTD_DATA_R = 0x0A;
}

void backward (){
  GPIO_PORTE_DATA_R = 0x05; // 0001 0101
  GPIO_PORTD_DATA_R = 0x05;
}

void turn_right (){
  GPIO_PORTE_DATA_R = 0x06; // 0010 0110   *
  GPIO_PORTD_DATA_R = 0x09;
}

void turn_left (){
  GPIO_PORTE_DATA_R = 0x09; //0001  1001
  GPIO_PORTD_DATA_R = 0x06;
}

void stop(){
  GPIO_PORTE_DATA_R = 0x0;
  GPIO_PORTD_DATA_R = 0x0;
}


void UART4_16MHz_Init() {
  SYSCTL_RCGCUART_R |= (1 << 4); // enable UART module 4
  SYSCTL_RCGC2_R |= (1<<2); // enable GPIO Port C
  GPIO_PORTC_DEN_R |= (1 << 4) | (1 << 5); // digital enable PC4, PC5
  GPIO_PORTC_AFSEL_R |= (1 << 4) | (1 << 5); // alternate function select for PC4, PC5
  GPIO_PORTC_PCTL_R |= (1 << 16) | (1 << 20); // configure port control to use UART
  UART4_CTL_R &= ~(1 << 0); // diable UART
  UART4_IBRD_R = 27; // integer portion
  UART4_FBRD_R = 9; // fractional portion
  UART4_LCRH_R |= (0x3 << 5) | (1 << 4); // set the word length to be 8 bits
  UART4_CC_R |= (0x0 << 0); // UART clock source set to be System clock
  UART4_CTL_R |= (1 << 0) | (1 << 8) | (1 << 9); // enable UART, TX and RX
}

char UART4_readChar(void){
  int count = 1;
  while((UART4_FR_R & (1<<4)) != 0){}
  char c = UART4_DR_R;  
  return c;
}

void UART4_printChar(char c){
  while ((UART4_FR_R & (1<<5)) != 0){}
  UART4_DR_R = c;
}

void UART4_printString(char* string){
  while (*string){
    UART4_printChar(*(string++));
  }
}

void car_init(void){
  SYSCTL_RCGC2_R |= SYSCTL_RCGC2_GPIOE | SYSCTL_RCGC2_GPIOF |SYSCTL_RCGC2_GPIOA | SYSCTL_RCGC2_GPIOD;
  GPIO_PORTE_AMSEL_R &= ~0x80;          // disable analog function on PA6 
  GPIO_PORTE_DEN_R |= 0x1 | 0x1 << 1 | 0x1 << 2 | 0x1 << 3 | 0x1 << 4 | 0x1 << 5;             // enable digital port 
  GPIO_PORTE_DIR_R |= 0x1 | 0x1 << 1 | 0x1 << 2 | 0x1 << 3 | 0x1 << 4 | 0x1 << 5;              // set direction on output
  GPIO_PORTE_PCTL_R &= ~0xFFFFFFFF;     // regular GPIO
  GPIO_PORTE_AFSEL_R &= ~0x03;          // regular port function
  GPIO_PORTE_PCTL_R &= ~0xFFFFFFFF;
  GPIO_PORTD_PCTL_R &= ~0xFFFFFFFF;     // regular GPIO
  GPIO_PORTD_AFSEL_R &= ~0x03;          // regular port function
  GPIO_PORTD_AMSEL_R &= ~0x80;   
  GPIO_PORTD_DEN_R |= 0x1 | 0x1 << 1| 0x1 << 2 | 0x1 << 3 ;             // enable digital port 
  GPIO_PORTD_DIR_R |= 0x1 | 0x1 << 1| 0x1 << 2 | 0x1 << 3 ;              // set direction on output
}           

uint32_t CarControlTaskInit(void)
{
    if(xTaskCreate(CarControlTask, (const portCHAR *)"CarControl",
                   CARTASKSTACKSIZE, NULL,  1, NULL) != pdTRUE)
    {
        return(1);
    }

    //
    // Success.
    //
    return(0);
}