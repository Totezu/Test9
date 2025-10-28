#include "Autotune.h"
#include "Sensors.h"
#include "Utils.h"

void startAutotune1() {
  readTemperatures();
  savedTargetTemp1 = targetTemp1;
  targetTemp1 = (int)(currentTemp1 + 10 + 0.5);
  autotuneInput1 = currentTemp1;
  autotuneOutput1 = 0.0;
  if (tuner1 != nullptr) { delete tuner1; tuner1 = nullptr; }
  tuner1 = new PID_ATune(&autotuneInput1, &autotuneOutput1);
  tuner1->SetOutputStep(3500);
  tuner1->SetLookbackSec(20);
  tuner1->SetNoiseBand(1);
  autotune1Active = true;
  autotune1PendingSave = false;
  autotune1Error = false;
  autotune1Start = millis();
  windowStartTime1 = millis();
#ifdef DEBUG_VERBOSE
  Serial.println("=== AUTOTUNE1 INIT LOG (PID_ATune) ===");
  Serial.print("currentTemp1: "); Serial.println(currentTemp1, 4);
  Serial.print("targetTemp1: "); Serial.println(targetTemp1);
  Serial.print("windowSize: "); Serial.println(windowSize);
#endif
  printEvent("Autotune1 started (PID_ATune)");
}

void startAutotune2() {
  readTemperatures();
  savedTargetTemp2 = targetTemp2;
  targetTemp2 = (int)(currentTemp2 + 10 + 0.5);
  autotuneInput2 = currentTemp2;
  autotuneOutput2 = 0.0;
  if (tuner2 != nullptr) { delete tuner2; tuner2 = nullptr; }
  tuner2 = new PID_ATune(&autotuneInput2, &autotuneOutput2);
  tuner2->SetOutputStep(3500);
  tuner2->SetLookbackSec(20);
  tuner2->SetNoiseBand(1);
  autotune2Active = true;
  autotune2PendingSave = false;
  autotune2Error = false;
  autotune2Start = millis();
  windowStartTime2 = millis();
#ifdef DEBUG_VERBOSE
  Serial.println("=== AUTOTUNE2 INIT LOG (PID_ATune) ===");
  Serial.print("currentTemp2: "); Serial.println(currentTemp2, 4);
  Serial.print("targetTemp2: "); Serial.println(targetTemp2);
  Serial.print("windowSize: "); Serial.println(windowSize);
#endif
  printEvent("Autotune2 started (PID_ATune)");
}

void updateAutotune() {
  static bool lastAutotuneHeater1State = false;
  static bool lastAutotuneHeater2State = false;

  unsigned long now = millis();

  if (autotune1Active && tuner1) {
    readTemperatures(); autotuneInput1 = currentTemp1;

    if (now - windowStartTime1 >= windowSize) {
      unsigned long windowsBehind = (now - windowStartTime1) / windowSize;
      windowStartTime1 += windowsBehind * windowSize;
      readTemperatures(); autotuneInput1 = currentTemp1;

      int tuneResult = tuner1->Runtime();

      if (tuneResult == 1) {
        autotuneKp1 = tuner1->GetKp(); autotuneKi1 = tuner1->GetKi(); autotuneKd1 = tuner1->GetKd();
        autotune1PendingSave = true; autotune1Active = false; autotune1Error = false;
        printEvent("T1 autotune awaiting confirmation (PID_ATune)");
        digitalWrite(HEATER1_PIN, LOW); lastAutotuneHeater1State = false;
        delete tuner1; tuner1 = nullptr;
      }
      if (now - autotune1Start > AUTOTUNE_TIMEOUT) {
        autotune1Active = false; autotune1Error = true;
        printEvent("T1 autotune TIMEOUT");
        digitalWrite(HEATER1_PIN, LOW); lastAutotuneHeater1State = false;
        delete tuner1; tuner1 = nullptr;
      }
    }

    bool autotuneHeater1State = ((now - windowStartTime1) < autotuneOutput1);
    digitalWrite(HEATER1_PIN, autotuneHeater1State ? HIGH : LOW);

    if (autotuneHeater1State != lastAutotuneHeater1State) {
      readTemperatures(); autotuneInput1 = currentTemp1;
#ifdef DEBUG_VERBOSE
      Serial.print("[AUTOTUNE1] Heater "); Serial.print(autotuneHeater1State ? "ON " : "OFF ");
      Serial.print(" Temp: "); Serial.print(autotuneInput1, 2);
      Serial.print(" Output: "); Serial.println(autotuneOutput1, 2);
#endif
      lastAutotuneHeater1State = autotuneHeater1State;
    }
  }

  if (autotune2Active && tuner2) {
    readTemperatures(); autotuneInput2 = currentTemp2;

    if (now - windowStartTime2 >= windowSize) {
      unsigned long windowsBehind = (now - windowStartTime2) / windowSize;
      windowStartTime2 += windowsBehind * windowSize;
      readTemperatures(); autotuneInput2 = currentTemp2;

      int tuneResult = tuner2->Runtime();

      if (tuneResult == 1) {
        autotuneKp2 = tuner2->GetKp(); autotuneKi2 = tuner2->GetKi(); autotuneKd2 = tuner2->GetKd();
        autotune2PendingSave = true; autotune2Active = false; autotune2Error = false;
        printEvent("T2 autotune awaiting confirmation (PID_ATune)");
        digitalWrite(HEATER2_PIN, LOW); lastAutotuneHeater2State = false;
        delete tuner2; tuner2 = nullptr;
      }
      if (now - autotune2Start > AUTOTUNE_TIMEOUT) {
        autotune2Active = false; autotune2Error = true;
        printEvent("T2 autotune TIMEOUT");
        digitalWrite(HEATER2_PIN, LOW); lastAutotuneHeater2State = false;
        delete tuner2; tuner2 = nullptr;
      }
    }

    bool autotuneHeater2State = ((now - windowStartTime2) < autotuneOutput2);
    digitalWrite(HEATER2_PIN, autotuneHeater2State ? HIGH : LOW);

    if (autotuneHeater2State != lastAutotuneHeater2State) {
      readTemperatures(); autotuneInput2 = currentTemp2;
#ifdef DEBUG_VERBOSE
      Serial.print("[AUTOTUNE2] Heater "); Serial.print(autotuneHeater2State ? "ON " : "OFF ");
      Serial.print(" Temp: "); Serial.print(autotuneInput2, 2);
      Serial.print(" Output: "); Serial.println(autotuneOutput2, 2);
#endif
      lastAutotuneHeater2State = autotuneHeater2State;
    }
  }
}