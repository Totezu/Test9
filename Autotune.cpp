#include "Autotune.h"
#include "Sensors.h"
#include "Utils.h"

// Helper to start autotune for channel 1/2 (no behavior changes)
static void startAutotuneCommon(
  // Text labels
  const char* initLogTitle,
  const char* startedMsg,
  // Channel variables
  bool& autotuneActive, bool& autotunePendingSave, bool& autotuneError,
  unsigned long& autotuneStart,
  double& autotuneInput, double& autotuneOutput,
  PID_ATune*& tuner,
  float& autotuneKp, float& autotuneKi, float& autotuneKd,
  int& savedTargetTemp, int& targetTemp,
  float currentTemp,
  unsigned long& windowStartTime,
  uint8_t /*heaterPin*/
) {
  readTemperatures();
  (void)initLogTitle; (void)autotuneKp; (void)autotuneKi; (void)autotuneKd; // silence unused-parameter warnings

  savedTargetTemp = targetTemp;
  targetTemp = (int)(currentTemp + 10 + 0.5);
  autotuneInput = currentTemp;
  autotuneOutput = 0.0;

  if (tuner != nullptr) { delete tuner; tuner = nullptr; }
  tuner = new PID_ATune(&autotuneInput, &autotuneOutput);
  tuner->SetOutputStep(3500);
  tuner->SetLookbackSec(20);
  tuner->SetNoiseBand(1);

  autotuneActive = true;
  autotunePendingSave = false;
  autotuneError = false;
  autotuneStart = millis();
  windowStartTime = millis();

#ifdef DEBUG_VERBOSE
  Serial.println(initLogTitle);
  Serial.print("currentTemp: "); Serial.println(currentTemp, 4);
  Serial.print("targetTemp: ");  Serial.println(targetTemp);
  Serial.print("windowSize: ");  Serial.println(windowSize);
#endif
  printEvent(startedMsg);
}

// Perform one autotune step for a channel (inside updateAutotune)
static void updateAutotuneOne(
  // Text / messages
  const char* dbgPrefix,
  const char* awaitingConfirmMsg,
  const char* timeoutMsg,
  // Channel flags/state
  bool& autotuneActive, bool& autotunePendingSave, bool& autotuneError,
  unsigned long& autotuneStart,
  double& autotuneInput, double& autotuneOutputRef,
  PID_ATune*& tuner,
  float& autotuneKp, float& autotuneKi, float& autotuneKd,
  // Measurements/window timers
  float currentTemp,
  unsigned long& windowStartTime,
  // External effects
  uint8_t heaterPin,
  bool isCh1
) {
  unsigned long now = millis();
  readTemperatures();
  autotuneInput = currentTemp;

  if (now - windowStartTime >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime) / windowSize;
    windowStartTime += windowsBehind * windowSize;

    readTemperatures();
    autotuneInput = currentTemp;

    int tuneResult = tuner->Runtime();

    if (tuneResult == 1) {
      autotuneKp = tuner->GetKp();
      autotuneKi = tuner->GetKi();
      autotuneKd = tuner->GetKd();
      autotunePendingSave = true;
      autotuneActive = false;
      autotuneError = false;
      printEvent(awaitingConfirmMsg);
      digitalWrite(heaterPin, LOW);
      delete tuner; tuner = nullptr;
    }
    if (now - autotuneStart > AUTOTUNE_TIMEOUT) {
      autotuneActive = false;
      autotuneError = true;
      printEvent(timeoutMsg);
      digitalWrite(heaterPin, LOW);
      delete tuner; tuner = nullptr;
    }
  }

  // SSR window PWM for autotune
  bool heaterState = ((now - windowStartTime) < (unsigned long)autotuneOutputRef);
  digitalWrite(heaterPin, heaterState ? HIGH : LOW);

  static bool lastAutotuneHeater1State = false;
  static bool lastAutotuneHeater2State = false;
  bool& lastStateRef = isCh1 ? lastAutotuneHeater1State : lastAutotuneHeater2State;

  if (heaterState != lastStateRef) {
    readTemperatures();
    autotuneInput = currentTemp;
#ifdef DEBUG_VERBOSE
    Serial.print(dbgPrefix); Serial.print(" Heater ");
    Serial.print(heaterState ? "ON " : "OFF ");
    Serial.print(" Temp: "); Serial.print(autotuneInput, 2);
    Serial.print(" Output: "); Serial.println(autotuneOutputRef, 2);
#endif
    lastStateRef = heaterState;
  }
}

void startAutotune1() {
  startAutotuneCommon(
    "=== AUTOTUNE1 INIT LOG (PID_ATune) ===",
    "Autotune1 started (PID_ATune)",
    autotune1Active, autotune1PendingSave, autotune1Error,
    autotune1Start,
    autotuneInput1, autotuneOutput1,
    tuner1,
    autotuneKp1, autotuneKi1, autotuneKd1,
    savedTargetTemp1, targetTemp1,
    currentTemp1,
    windowStartTime1,
    HEATER1_PIN
  );
}

void startAutotune2() {
  startAutotuneCommon(
    "=== AUTOTUNE2 INIT LOG (PID_ATune) ===",
    "Autotune2 started (PID_ATune)",
    autotune2Active, autotune2PendingSave, autotune2Error,
    autotune2Start,
    autotuneInput2, autotuneOutput2,
    tuner2,
    autotuneKp2, autotuneKi2, autotuneKd2,
    savedTargetTemp2, targetTemp2,
    currentTemp2,
    windowStartTime2,
    HEATER2_PIN
  );
}

void updateAutotune() {
  if (autotune1Active && tuner1) {
    updateAutotuneOne(
      "[AUTOTUNE1]",
      "T1 autotune awaiting confirmation (PID_ATune)",
      "T1 autotune TIMEOUT",
      autotune1Active, autotune1PendingSave, autotune1Error,
      autotune1Start,
      autotuneInput1, autotuneOutput1,
      tuner1,
      autotuneKp1, autotuneKi1, autotuneKd1,
      currentTemp1,
      windowStartTime1,
      HEATER1_PIN,
      true
    );
  }

  if (autotune2Active && tuner2) {
    updateAutotuneOne(
      "[AUTOTUNE2]",
      "T2 autotune awaiting confirmation (PID_ATune)",
      "T2 autotune TIMEOUT",
      autotune2Active, autotune2PendingSave, autotune2Error,
      autotune2Start,
      autotuneInput2, autotuneOutput2,
      tuner2,
      autotuneKp2, autotuneKi2, autotuneKd2,
      currentTemp2,
      windowStartTime2,
      HEATER2_PIN,
      false
    );
  }
}