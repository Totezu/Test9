#pragma once
#include "Config.h"
#include <Adafruit_MAX31856.h>
#include <U8g2lib.h>
#include <PID_v1.h>

// MAX31856
extern Adafruit_MAX31856 max1;
extern Adafruit_MAX31856 max2;

// Display
#if USE_DISPLAY
// Было: extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C display;
// Стало: постраничный буфер (2‑page) — экономит ~1 КБ SRAM
extern U8G2_SSD1306_128X64_NONAME_2_HW_I2C display;
#endif

// Buttons
extern const uint8_t btnPins[10];

// Core state
extern int targetTemp1;
extern int targetTemp2;
extern int holdMinutes;

extern float currentTemp1;
extern float currentTemp2;

extern int startTemp1;
extern int startTemp2;

extern bool sensorsReady;

// Autotune I/O
extern double autotuneInput1, autotuneOutput1;
extern double autotuneInput2, autotuneOutput2;

extern int savedTargetTemp1;
extern int savedTargetTemp2;

extern unsigned long lastAutotuneLog;
extern const unsigned long autotuneLogPeriod;
extern unsigned long lastDebugLog;
extern const unsigned long debugLogPeriod;

// Debug flags
extern bool sensorsDebug;

// Web interface enable/disable
extern bool webEnabled;

extern unsigned long heatingStartTime;
extern unsigned long holdingStartTime;
extern bool heatingPhase, holdingPhase, complete, processActive;
extern unsigned long remainSeconds;

// Runtime-configurable timing values (lowercase to avoid macro name conflicts)
extern unsigned long one_minute_ms;
extern unsigned long autotune_timeout;

#ifndef ONE_MINUTE_MS
  #define ONE_MINUTE_MS (one_minute_ms)
#endif
#ifndef AUTOTUNE_TIMEOUT
  #define AUTOTUNE_TIMEOUT (autotune_timeout)
#endif

// Allowed stepper
extern int allowedT1;
extern int allowedT2;
extern unsigned long lastAllowedUpdate1;
extern unsigned long lastAllowedUpdate2;
extern unsigned long eqHoldStart1;
extern unsigned long eqHoldStart2;
extern unsigned long accumHoldMs1;
extern unsigned long accumHoldMs2;
extern unsigned long lastLoopTs;

extern int lastCompletedGlobal;
extern unsigned long lastAllowedChangeTime;
extern uint8_t displayVirtualConsumed;

// Align phase
extern bool alignPhase;

// Buttons debounce
extern bool btnPrev[10];
extern unsigned long lastBtnTime;
extern const unsigned long debounceDelay;

// Timings
extern unsigned long lastTempRead;
extern unsigned long lastScreenUpdate;
extern const unsigned long tempPeriod;
extern const unsigned long screenPeriod;

// PID params and objects
extern double Kp1, Ki1, Kd1;
extern double Kp2, Ki2, Kd2;

extern double Input1, Output1, Setpoint1;
extern double Input2, Output2, Setpoint2;

extern PID pid1;
extern PID pid2;

// Autotune
struct PID_ATune;
extern PID_ATune* tuner1;
extern PID_ATune* tuner2;
extern bool autotune1Active, autotune2Active;
extern unsigned long autotune1Start, autotune2Start;

// Autotune results
extern float autotuneKp1, autotuneKi1, autotuneKd1;
extern float autotuneKp2, autotuneKi2, autotuneKd2;
extern bool autotune1PendingSave;
extern bool autotune2PendingSave;
extern bool autotune1Error;
extern bool autotune2Error;

// Offsets
extern float temp1_offset, temp2_offset;

// SSR windowing
extern unsigned long windowStartTime1;
extern double pidOutput1;
extern unsigned long windowStartTime2;
extern double pidOutput2;

// Phase markers
extern unsigned long heatEntryTime;
extern unsigned long lastAlignComplete;

// Last-good raw temps
extern float lastGoodTemp1;
extern float lastGoodTemp2;

// Relays
extern bool heater1State, heater2State;

// Logging caches
extern int lastLoggedTemp1;
extern int lastLoggedTemp2;
extern int lastLoggedTarget1;
extern int lastLoggedTarget2;

// Debug prev
extern int   lastAllowedT1;
extern int   lastAllowedT2;
extern float lastCurrentT1;
extern float lastCurrentT2;
extern float lastDiffT1;
extern float lastDiffT2;
extern float lastOutput1;
extern float lastOutput2;
extern bool  lastHeatingPhase;
extern bool  lastHoldingPhase;
extern bool  lastProcessActive;
extern bool  lastHeater1State;
extern bool  lastHeater2State;

// Sensors (hot-plug)
extern bool max1Found;
extern bool max2Found;
extern uint8_t max1ErrorCount;
extern uint8_t max2ErrorCount;
extern unsigned long lastSensorCheck;
extern const unsigned long sensorCheckPeriod;

// DHCP success flag for UI (true if IP obtained via DHCP during init)
extern bool netDhcpSuccess;

// Serial console helper (implemented in Globals.cpp)
void processSerialConsole();