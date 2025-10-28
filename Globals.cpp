#include "Globals.h"
#include "WebInterface.h"

// MAX31856
Adafruit_MAX31856 max1(MAX1_CS);
Adafruit_MAX31856 max2(MAX2_CS);

// Display
#if USE_DISPLAY
// Было: U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(...);
// Стало: постраничный буфер (2‑page) — уменьшает потребление SRAM ~на 1 КБ
U8G2_SSD1306_128X64_NONAME_2_HW_I2C display(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
#endif

// Buttons
const uint8_t btnPins[10] = {
  BTN1_PLUS, BTN1_MINUS, BTN2_PLUS, BTN2_MINUS,
  BTN_TIME_PLUS, BTN_TIME_MINUS, BTN_START, BTN_STOP,
  BTN1_AUTOTUNE, BTN2_AUTOTUNE
};

// Core state defaults
int targetTemp1 = 31;
int targetTemp2 = 31;
int holdMinutes = 60;

float currentTemp1 = NAN, currentTemp2 = NAN;

int startTemp1 = 0, startTemp2 = 0;

bool sensorsReady = false;

// Autotune I/O
double autotuneInput1 = 23.4, autotuneOutput1 = 800.0;
double autotuneInput2 = 21.5, autotuneOutput2 = 800.0;

int savedTargetTemp1 = 0;
int savedTargetTemp2 = 0;

unsigned long lastAutotuneLog = 0;
const unsigned long autotuneLogPeriod = 500;
unsigned long lastDebugLog = 0;
const unsigned long debugLogPeriod = 500;

// Debug flags
bool sensorsDebug = false;

// Web flag (default ON)
bool webEnabled = true;

unsigned long heatingStartTime = 0;
unsigned long holdingStartTime = 0;
bool heatingPhase = false, holdingPhase = false, complete = false, processActive = false;
unsigned long remainSeconds = 0;

#ifndef DEFAULT_ONE_MINUTE_MS
  #define DEFAULT_ONE_MINUTE_MS 60000UL
#endif
#ifndef DEFAULT_AUTOTUNE_TIMEOUT
  #define DEFAULT_AUTOTUNE_TIMEOUT 300000UL
#endif

unsigned long one_minute_ms = DEFAULT_ONE_MINUTE_MS;
unsigned long autotune_timeout = DEFAULT_AUTOTUNE_TIMEOUT;

// Allowed stepper
int allowedT1 = 0;
int allowedT2 = 0;
unsigned long lastAllowedUpdate1 = 0;
unsigned long lastAllowedUpdate2 = 0;
unsigned long eqHoldStart1 = 0;
unsigned long eqHoldStart2 = 0;
unsigned long accumHoldMs1 = 0;
unsigned long accumHoldMs2 = 0;
unsigned long lastLoopTs = 0;

int lastCompletedGlobal = 0;
unsigned long lastAllowedChangeTime = 0;
uint8_t displayVirtualConsumed = 0;

// Align phase
bool alignPhase = false;

// Buttons debounce
bool btnPrev[10] = {0};
unsigned long lastBtnTime = 0;
const unsigned long debounceDelay = 50;

// Timings
unsigned long lastTempRead = 0;
unsigned long lastScreenUpdate = 0;
const unsigned long tempPeriod = 500;
const unsigned long screenPeriod = 350;

// PID
double Kp1 = DEFAULT_KP1, Ki1 = DEFAULT_KI1, Kd1 = DEFAULT_KD1;
double Kp2 = DEFAULT_KP2, Ki2 = DEFAULT_KI2, Kd2 = DEFAULT_KD2;

double Input1 = 0, Output1 = 0, Setpoint1 = 0;
double Input2 = 0, Output2 = 0, Setpoint2 = 0;

// PID objects
PID pid1(&Input1, &Output1, &Setpoint1, Kp1, Ki1, Kd1, DIRECT);
PID pid2(&Input2, &Output2, &Setpoint2, Kp2, Ki2, Kd2, DIRECT);

// Autotune
PID_ATune* tuner1 = nullptr;
PID_ATune* tuner2 = nullptr;
bool autotune1Active = false, autotune2Active = false;
unsigned long autotune1Start = 0, autotune2Start = 0;

// Autotune results
float autotuneKp1 = 0, autotuneKi1 = 0, autotuneKd1 = 0;
float autotuneKp2 = 0, autotuneKi2 = 0, autotuneKd2 = 0;
bool autotune1PendingSave = false;
bool autotune2PendingSave = false;
bool autotune1Error = false;
bool autotune2Error = false;

// Offsets
float temp1_offset = 0.0, temp2_offset = 0.0;

// SSR windowing
unsigned long windowStartTime1 = 0;
double pidOutput1 = 0;
unsigned long windowStartTime2 = 0;
double pidOutput2 = 0;

// Phase markers
unsigned long heatEntryTime = 0;
unsigned long lastAlignComplete = 0;

// Last-good raw temps
float lastGoodTemp1 = NAN;
float lastGoodTemp2 = NAN;

// Relays
bool heater1State = false, heater2State = false;

// Logging caches
int lastLoggedTemp1 = -9999;
int lastLoggedTemp2 = -9999;
int lastLoggedTarget1 = -9999;
int lastLoggedTarget2 = -9999;

// Debug prev
int   lastAllowedT1 = -9999;
int   lastAllowedT2 = -9999;
float lastCurrentT1 = NAN;
float lastCurrentT2 = NAN;
float lastDiffT1 = NAN;
float lastDiffT2 = NAN;
float lastOutput1 = NAN;
float lastOutput2 = NAN;
bool  lastHeatingPhase = false;
bool  lastHoldingPhase = false;
bool  lastProcessActive = false;
bool  lastHeater1State = false;
bool  lastHeater2State = false;

// Sensors (hot-plug)
bool max1Found = false;
bool max2Found = false;
uint8_t max1ErrorCount = 0;
uint8_t max2ErrorCount = 0;
unsigned long lastSensorCheck = 0;
const unsigned long sensorCheckPeriod = 1000;

// DHCP success flag
bool netDhcpSuccess = false;

// Simple serial console processor (WEB/SENS toggle etc.)
void processSerialConsole() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c != '\n') { if (buf.length() < 160) buf += c; continue; }

    String cmd = buf; buf = "";
    cmd.trim();
    if (cmd.length() == 0) return;
    String up = cmd; up.toUpperCase();

    if (up == "WEB ON")      { setWebEnabled(true); Serial.println("[WEB] ENABLED"); continue; }
    if (up == "WEB OFF")     { setWebEnabled(false); Serial.println("[WEB] DISABLED"); continue; }
    if (up == "WEB TOGGLE")  { setWebEnabled(!webEnabled); Serial.println("[WEB] TOGGLE"); continue; }
    if (up == "SENS ON")     { sensorsDebug = true; Serial.println("[SENS] debug ENABLED"); continue; }
    if (up == "SENS OFF")    { sensorsDebug = false; Serial.println("[SENS] debug DISABLED"); continue; }
    if (up == "STATUS") {
      Serial.print(F("[WEB] ")); Serial.println(webEnabled ? F("ENABLED") : F("DISABLED"));
      Serial.print(F("[SENS] debug ")); Serial.println(sensorsDebug ? F("ON") : F("OFF"));
      Serial.print(F("[one_minute_ms] ")); Serial.println(one_minute_ms);
      Serial.print(F("[autotune_timeout] ")); Serial.println(autotune_timeout);
      continue;
    }

    Serial.print(F("[CMD] Unknown: ")); Serial.println(cmd);
  }
}