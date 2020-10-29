#ifndef __TIVA_H
#define __TIVA_H

#include <stdint.h>

#define LED_BASE GPIO_PORTF_BASE
#define RED_LED     GPIO_PIN_1
#define BLUE_LED    GPIO_PIN_2
#define GREEN_LED   GPIO_PIN_3
#define YELLOW_LED  (RED_LED | GREEN_LED)
#define MAGENTA_LED (RED_LED | BLUE_LED)
#define CYAN_LED    (GREENLED | BLUE_LED)
#define WHITE_LED   (RED_LED | GREEN_LED | BLUE_LED)

#define USER_BUTTON1 GPIO_PORTF_BASE,GPIO_PIN_0
#define USER_BUTTON2 GPIO_PORTF_BASE,GPIO_PIN_4

void TIVA_DFU(void);


#endif // ifndef __TIVA_H
