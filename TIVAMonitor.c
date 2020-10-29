#include "TIVAMonitor.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "inc/hw_memmap.h"
#include "driverlib/debug.h"
#include "driverlib/fpu.h"
#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/timer.h"
#include "driverlib/uart.h"
#include "driverlib/usb.h"
#include "driverlib/pwm.h"
#include "driverlib/rom.h"
#include "driverlib/eeprom.h"
#include "usblib/usblib.h"
#include "usblib/usbcdc.h"
#include "usblib/usb-ids.h"
#include "usblib/device/usbdevice.h"
#include "usblib/device/usbdcdc.h"
#include "utils/ustdlib.h"
#include "usb_serial_structs.h"
#include "utils/uartstdio.h"
#include "driverlib/ssi.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "Tiva.h"
#include "Helpers.h"
#include "Tiva.h"
#include "USBSerial.h"
#include "CommandProcessor.h"
#include "EEPROM.h"
#include "Init.h"
#include "inc/hw_i2c.h"
#include "driverlib/i2c.h"
#include "driverlib/rom_map.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
const unsigned char FWVersion[] = {"Project FW version 1.00"};
const unsigned char BuildDate[] = "Build date = " __DATE__;
const unsigned char BuildTime[] = "Build Time = " __TIME__;

uint32_t SerialNumber = 0;
DeviceInfoType DeviceInfo;

unsigned char MiscBuffer[64];

uint8_t Mode = MODE_RPM;
uint8_t CaptureEnable = 0;
uint16_t CaptureInterval = 10;
volatile uint64_t Delta[2] = {0, 0};
uint8_t ChangeTrack[2] = {0, 0};
uint8_t DebugOutput = 0;
uint64_t DebounceCount = ((int) 2000000/12.5);
uint64_t LastTimerValue[2] = {0, 0};
uint64_t TimeTrack = 0;

///////////////////////////////////////////////////////////////////////////////////////////////////////


//*****************************************************************************
//
// Interrupt handler for the system tick counter.
//
//*****************************************************************************
void SysTickIntHandler(void)
{
    if (DelayCounter > 0)
    {
        DelayCounter--;
    }
}

void DisplayIntervals()
{
    static uint64_t LocalDelta[2] = {0, 0};
    uint8_t Loop;
    uint8_t LEDCount = 0;

    TimeTrack = 0;
    USBConnected = true;
    USBSerial_SendMessage((unsigned char *)"\n\nTime,   Motor 1,  Motor2\n");
    while ((CommandCount == 0) && (GPIOPinRead(USER_BUTTON1) != 0))
    {
        if (USBConnected)
        {
            if (LEDCount & 4)
                MAP_GPIOPinWrite(LED_BASE, RED_LED|BLUE_LED|GREEN_LED, GREEN_LED);
            else
                MAP_GPIOPinWrite(LED_BASE, RED_LED|BLUE_LED|GREEN_LED, 0);
        }
        else
        {
            if (LEDCount & 4)
                MAP_GPIOPinWrite(LED_BASE, RED_LED|BLUE_LED|GREEN_LED, RED_LED);
            else
                MAP_GPIOPinWrite(LED_BASE, RED_LED|BLUE_LED|GREEN_LED, 0);
        }
        LEDCount++;

        for (Loop = 0; Loop < 2; Loop ++)
        {
            if (ChangeTrack[Loop] <= 20)
            {
                LocalDelta[Loop] = Delta[Loop];
                ChangeTrack[Loop]++;
            }
            else
            {
                LocalDelta[Loop] =  LocalDelta[Loop] + (CaptureInterval * 800000);
            }
        }
        if (Mode == MODE_TIME)
        {
//            sprintf((char*)MiscBuffer, "% 4.4f\tms\t% 4.4f\tms\n", (float)LocalDelta[0] / 80000.0, (float)LocalDelta[1] / 80000.0);
            sprintf((char*)MiscBuffer, "%3.2f, % 4.4f, % 4.4f, ms\n", TimeTrack / 1000.0, (float)LocalDelta[0] / 80000.0, (float)LocalDelta[1] / 80000.0);
        }
        else
        {
//            sprintf((char*)MiscBuffer, "% 6llu\trpm\t% 6llu\trpm\n", (uint64_t)((float)600000000.0 * 8.0)/LocalDelta[0], (uint64_t)((float)600000000.0 * 8.0)/LocalDelta[1]);
            sprintf((char*)MiscBuffer, "%3.2f, % 6llu, % 6llu, rpm\n", TimeTrack / 1000.0, (uint64_t)((float)600000000.0 * 8.0)/LocalDelta[0], (uint64_t)((float)600000000.0 * 8.0)/LocalDelta[1]);
        }
        USBSerial_SendMessage((unsigned char *)MiscBuffer);
        WaitFormS(CaptureInterval);
        TimeTrack = TimeTrack + CaptureInterval;
    }
    MAP_GPIOPinWrite(LED_BASE, RED_LED|BLUE_LED|GREEN_LED, 0);
}

void ProcessCounterTrigger(uint8_t Channel)
{
    uint64_t CurrentTimerValue;
    uint64_t NextDelta;
    static uint64_t Debounce[2] = {0, 0};

    CurrentTimerValue = TimerValueGet64(IR_TIMER_BASE);
    NextDelta = (CurrentTimerValue - LastTimerValue[Channel]);

    if (NextDelta > 80000000)//This is a startup
    {
        //We have no idea how fast we are about to spin, but set s threshold at something 'reasonable'
        NextDelta = 80000000;
        Debounce[Channel] = 2000000;
    }

    //Filter to only durations that are at least half of the longest to date, slowly reducing the threshold if not found
    //This is needed because the 'detector' strip causes pulses at both edges of the tape due to noise
    //We basically need to filter the small 'strip' period
    if (NextDelta > Debounce[Channel])
    {
        Debounce[Channel] = NextDelta >> 2;//Track the longest pulse
        Delta[Channel] = NextDelta;
        LastTimerValue[Channel] = CurrentTimerValue;
        ChangeTrack[Channel] = 0;
//        sprintf((char*)MiscBuffer, "%9llu %9llu\n", NextDelta, CurrentTimerValue);
//        USBSerial_SendMessage((unsigned char *)MiscBuffer);
    }
    else
        Debounce[Channel] = Debounce[Channel] * 0.95;

}

void PortFIntHandler()
{
    uint32_t status=0;

    status = GPIOIntStatus(GPIO_PORTF_BASE,true);
    GPIOIntClear(GPIO_PORTF_BASE,status);
}

void PortAIntHandler()
{
    uint32_t status=0;
    uint8_t PinState;

    status = GPIOIntStatus(GPIO_PORTA_BASE,true);

    //Disabling and re-enabling interrupts seems to be enough time to debounce
    GPIOIntDisable(GPIO_PORTA_BASE, GPIO_INT_PIN_7 | GPIO_INT_PIN_6);

//    PinState = GPIOPinRead(GPIO_PORTA_BASE, 0xff);

    if( (status & GPIO_INT_PIN_7) != 0)
        ProcessCounterTrigger(0);
//    ProcessCounterTrigger(0, PinState & GPIO_PIN_7);
    if( (status & GPIO_INT_PIN_6) != 0)
        ProcessCounterTrigger(1);
//        ProcessCounterTrigger(1, PinState & GPIO_PIN_6);

    GPIOIntClear(GPIO_PORTA_BASE,status);
    GPIOIntEnable(GPIO_PORTA_BASE, GPIO_INT_PIN_7 | GPIO_INT_PIN_6);

}

//*****************************************************************************
//
// This is the main application entry function.
//
//*****************************************************************************

int main(void)
{
    //Initialize stack, clocks and system ticker
    Init_SystemInit();
  
    //Initialize TIVA sub-systems
    Init_PeripheralInit();

    while (1)
    {
        ComProc_ProcessCommand();
        if (GPIOPinRead(USER_BUTTON1) == 0)
        {
            WaitFormS(50);
            while(GPIOPinRead(USER_BUTTON1) == 0);
            WaitFormS(50);
            DisplayIntervals();
            while(GPIOPinRead(USER_BUTTON1) == 0);
            WaitFormS(50);
        }
    }
}
