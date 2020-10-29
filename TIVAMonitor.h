#ifndef __TIVAMONITOR_H
#define __TIVAMONITOR_H

#include <stdint.h>
#include "EEPROM.h"

//*****************************************************************************
//
// The system tick rate expressed both as ticks per second and a millisecond
// period.
//
//*****************************************************************************
#define SYSTICKS_PER_SECOND 1000
#define SYSTICK_PERIOD_MS (1000 / SYSTICKS_PER_SECOND)

#define MODE_TIME 0
#define MODE_RPM  1

#define DISPLAY_REVS      0
#define DISPLAY_TIME      1

#define VALID_KEY 0x2869ea83
#define INVALID_KEY 0xffffffff
extern const unsigned char FWVersion[];
extern const unsigned char BuildDate[];
extern const unsigned char BuildTime[];
extern uint32_t SerialNumber;
extern DeviceInfoType DeviceInfo;
extern uint8_t Mode;
extern uint8_t DebugOutput;
extern uint8_t CaptureEnable;
extern uint16_t CaptureInterval;
extern volatile uint64_t Delta[2];
extern uint64_t DebounceCount;

extern unsigned char MiscBuffer[];
extern void PortAIntHandler();
extern void DisplayIntervals();
extern void ProcessCounterTrigger(uint8_t Channel);
//extern void ProcessCounterTrigger(uint8_t Channel, uint8_t State);

#endif // ifndef __TIVAMONITOR_H
