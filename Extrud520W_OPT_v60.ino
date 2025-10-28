#include "Config.h"
#include "Globals.h"

#include "Utils.h"
#include "Sensors.h"
#include "Display.h"
#include "Buttons.h"
#include "EepromStore.h"
#include "Autotune.h"
#include "Process.h"
#include "Control.h"
#include "WebInterface.h"   // NEW: web server

#include "Debug.h" // DEBUG_VERBOSE controls detailed Serial output

// Persistent flags for hold-timer readiness in HEAT
static bool eqHoldReady1 = false;
static bool eqHoldReady2 = false;

// New: explicit heating progress tracking (local to this file)
static unsigned long lastStepTime = 0; // ms of last recorded step
static int stepsCompleted = 0;         // steps completed since startTemp

// Track last drawn remainSeconds on screen to force redraws when it changes
static long lastDrawnRemainSeconds = -1;

// Keep these helpers local to this file
void updateStepsCompletedFromAllowed(unsigned long now) {
  int s1 = allowedT1 - startTemp1 - 1; if (s1 < 0) s1 = 0;
  int s2 = allowedT2 - startTemp2 - 1; if (s2 < 0) s2 = 0;
  bool ch1Done = (allowedT1 >= targetTemp1);
  bool ch2Done = (allowedT2 >= targetTemp2);
  int newCompleted;
  if (!ch1Done && !ch2Done) newCompleted = min(s1, s2);
  else if (ch1Done && !ch2Done) newCompleted = s2;
  else if (!ch1Done && ch2Done) newCompleted = s1;
  else newCompleted = max(s1, s2);
  if (newCompleted < 0) newCompleted = 0;

  if (newCompleted != stepsCompleted) {
    stepsCompleted = newCompleted;
    lastStepTime = now;
    // keep globals in sync
    lastCompletedGlobal = stepsCompleted;
    lastAllowedChangeTime = lastStepTime;
    displayVirtualConsumed = 0;
  }
}

unsigned long computeHeatRemainingMs(unsigned long now) {
  int totalSteps1 = targetTemp1 - startTemp1;
  int totalSteps2 = targetTemp2 - startTemp2;
  int totalSteps = max(totalSteps1, totalSteps2);
  if (totalSteps <= 0) return 0UL;

  int stepsLeft = totalSteps - stepsCompleted;
  if (stepsLeft <= 0) return 0UL;

  if (lastStepTime == 0) {
    return (unsigned long)stepsLeft * ONE_MINUTE_MS;
  } else {
    long long rem = (long long)stepsLeft * (long long)ONE_MINUTE_MS - (long long)(now - lastStepTime);
    if (rem <= 0) return 0UL;
    return (unsigned long)rem;
  }
}

void setup() {
  Wire.begin();
  SPI.begin();
  pinMode(SS, OUTPUT);
  deassertOtherSPI();

  Serial.begin(115200);
  // убрал blocking delay(150) — не требуется в setup для нормальной работы

  // Для ESP: убедитесь, что EEPROM.begin(...) выполнен раньше (см. ниже)
  performOneTimeEEPROMClear();   // <-- Должно быть ПЕРЕД loadConfigFromEEPROM()
  
  // load persisted config (one_minute_ms, autotune_timeout) before web init
  loadConfigFromEEPROM();
  
  // --- Web server: try to bring it up (uses standard SS if available) ---
  webSetup();

  // Sensors / PID / display initialization (kept as in your project)
  max1Found = beginMAX(max1);
#ifdef DEBUG_VERBOSE
  if (!max1Found) Serial.println("Could not init MAX31856 #1 (CS=42)!");
#endif
  max2Found = beginMAX(max2);
#ifdef DEBUG_VERBOSE
  if (!max2Found) Serial.println("Could not init MAX31856 #2 (CS=44)!");
#endif
  sensorsReady = max1Found && max2Found;

  pinMode(HEATER1_PIN, OUTPUT); digitalWrite(HEATER1_PIN, LOW);
  pinMode(HEATER2_PIN, OUTPUT); digitalWrite(HEATER2_PIN, LOW);

  setupButtons();

#if USE_DISPLAY
  display.begin(); display.clearBuffer(); display.sendBuffer();
#endif

  loadPIDFromEEPROM(Kp1, Ki1, Kd1, EEPROM_ADDR_KP1, DEFAULT_KP1, DEFAULT_KI1, DEFAULT_KD1, "PID1");
  pid1.SetTunings(Kp1, Ki1, Kd1);
  pid1.SetMode(AUTOMATIC);

  loadPIDFromEEPROM(Kp2, Ki2, Kd2, EEPROM_ADDR_KP2, DEFAULT_KP2, DEFAULT_KI2, DEFAULT_KD2, "PID2");
  pid2.SetTunings(Kp2, Ki2, Kd2);
  pid2.SetMode(AUTOMATIC);

  if (!sensorsReady) {
    allowedT1 = allowedT2 = -1;
    drawScreen(-1, -1);
    lastScreenUpdate = millis();
  } else {
    readTemperatures();
    allowedT1 = (int)(currentTemp1 + 0.5) + 1;
    allowedT2 = (int)(currentTemp2 + 0.5) + 1;
    drawScreen(allowedT1, allowedT2);
    lastScreenUpdate = millis();
  }

  loadTempOffsets();

  // Previously unconditional: holdMinutes = 60;
  // Now only set default 60 if EEPROM-loaded value is 0 (treat 0 as "not set")
  if (holdMinutes == 0) {
    holdMinutes = 60;
  }

  lastLoggedTemp1 = lastLoggedTemp2 = -9999;
  lastLoggedTarget1 = lastLoggedTarget2 = -9999;

  lastAllowedT1 = lastAllowedT2 = -9999;
  lastCurrentT1 = lastCurrentT2 = NAN;
  lastDiffT1 = lastDiffT2 = NAN;
  lastOutput1 = lastOutput2 = NAN;
  lastHeatingPhase = lastHoldingPhase = lastProcessActive = false;
  lastHeater1State = lastHeater2State = false;

  heatEntryTime = 0;

  eqHoldReady1 = eqHoldReady2 = false;
  lastStepTime = 0;
  stepsCompleted = 0;
  lastDrawnRemainSeconds = -1;
}

void loop() {
  unsigned long now = millis();

  // Serve web requests if enabled
  webLoop();

  // Serial console commands (enable/disable web, debug)
  processSerialConsole();

  if (now - lastSensorCheck >= sensorCheckPeriod) {
    checkSensors();
    lastSensorCheck = now;
  }

  handleButtonsAutoInc();
  checkButtons();

  if (!sensorsReady) {
    digitalWrite(HEATER1_PIN, LOW);
    digitalWrite(HEATER2_PIN, LOW);
    heater1State = false;
    heater2State = false;

    if (now - lastScreenUpdate >= screenPeriod) {
      drawScreen(-1, -1);
      lastScreenUpdate = now;
    }
    SIM_DELAY(100);
    return;
  }

  if (now - lastTempRead >= tempPeriod) {
    readTemperatures();
    lastTempRead = now;
    logTempsIfChanged();
  }

  // When no process is running, keep allowed following current temps (+1)
  if (!processActive && sensorsReady) {
    int newAllowed1 = (int)(currentTemp1 + 0.5) + 1;
    int newAllowed2 = (int)(currentTemp2 + 0.5) + 1;
    if (allowedT1 != newAllowed1 || allowedT2 != newAllowed2) {
      allowedT1 = newAllowed1;
      allowedT2 = newAllowed2;
      lastAllowedChangeTime = 0;
      lastCompletedGlobal = 0;
      displayVirtualConsumed = 0;
      eqHoldStart1 = eqHoldStart2 = 0;
      eqHoldReady1 = eqHoldReady2 = false;
      drawScreen(allowedT1, allowedT2);
      lastScreenUpdate = millis();
    }
  }

  // Compute remain for current phase (ALIGN/HOLD will be correct here; HEAT refines below)
  calcRemainTime();

  // --- Diagnostics + Phase control ---
  if (processActive) {
    int intT1 = (int)(currentTemp1 + 0.5);
    int intT2 = (int)(currentTemp2 + 0.5);
    char leftBuf[16]; formatRemain(remainSeconds, leftBuf, sizeof(leftBuf));
#ifdef DEBUG_VERBOSE
    Serial.print("[DBG] loop now="); Serial.print(now);
    Serial.print(" LEFT="); Serial.print(leftBuf);
    Serial.print(" curT1="); Serial.print(currentTemp1,2);
    Serial.print(" intT1="); Serial.print(intT1);
    Serial.print(" allowedT1="); Serial.print(allowedT1);
    Serial.print(" curT2="); Serial.print(currentTemp2,2);
    Serial.print(" intT2="); Serial.print(intT2);
    Serial.print(" allowedT2="); Serial.println(allowedT2);
#endif

    if (heatingPhase && !alignPhase) {
      // HEAT gate: require signed diff and step-balance to be correct, else return to ALIGN
      int dAllowed = allowedT1 - allowedT2;
      int dTarget  = targetTemp1 - targetTemp2;
      int stepsLeft1 = (targetTemp1 > allowedT1) ? (targetTemp1 - allowedT1) : 0;
      int stepsLeft2 = (targetTemp2 > allowedT2) ? (targetTemp2 - allowedT2) : 0;
      bool signedDiffOk  = (dAllowed == dTarget);
      bool stepsBalanced = (stepsLeft1 == stepsLeft2);

      if (!(signedDiffOk && stepsBalanced)) {
        alignPhase = true; heatingPhase = false;
#ifdef DEBUG_VERBOSE
        Serial.print("[DBG] heatingPhase -> alignPhase because ");
        if (!signedDiffOk)  Serial.print("signed diff mismatch; ");
        if (!stepsBalanced) Serial.print("stepsLeft mismatch; ");
        Serial.print("dAllowed="); Serial.print(dAllowed);
        Serial.print(" dTarget=");  Serial.print(dTarget);
        Serial.print(" stepsLeft1="); Serial.print(stepsLeft1);
        Serial.print(" stepsLeft2="); Serial.println(stepsLeft2);
#endif
      } else {
        // Stepwise HEAT control (kept unchanged)
        bool need1 = (allowedT1 < targetTemp1);
        bool need2 = (allowedT2 < targetTemp2);

        if (need1 && eqHoldStart1 == 0) eqHoldStart1 = now;
        if (need2 && eqHoldStart2 == 0) eqHoldStart2 = now;

        if (need1 && !eqHoldReady1 && (now - eqHoldStart1) >= ONE_MINUTE_MS) {
          eqHoldReady1 = true;
#ifdef DEBUG_VERBOSE
          Serial.println("[DBG] eqHoldReady1 set TRUE (timer elapsed)");
#endif
        }
        if (need2 && !eqHoldReady2 && (now - eqHoldStart2) >= ONE_MINUTE_MS) {
          eqHoldReady2 = true;
#ifdef DEBUG_VERBOSE
          Serial.println("[DBG] eqHoldReady2 set TRUE (timer elapsed)");
#endif
        }

        if (!need1) { eqHoldReady1 = false; eqHoldStart1 = 0; }
        if (!need2) { eqHoldReady2 = false; eqHoldStart2 = 0; }

        bool timerReady1 = need1 ? eqHoldReady1 : false;
        bool timerReady2 = need2 ? eqHoldReady2 : false;

        bool tempOk1 = need1 ? (currentTemp1 >= (float)allowedT1) : false;
        bool tempOk2 = need2 ? (currentTemp2 >= (float)allowedT2) : false;

#ifdef DEBUG_VERBOSE
        Serial.print("[DBG-HEAT] eq1="); Serial.print(eqHoldStart1);
        Serial.print(" eq2="); Serial.print(eqHoldStart2);
        Serial.print(" ready1="); Serial.print(eqHoldReady1);
        Serial.print(" ready2="); Serial.print(eqHoldReady2);
        Serial.print(" need1="); Serial.print(need1);
        Serial.print(" need2="); Serial.print(need2);
        Serial.print(" tReady1="); Serial.print(timerReady1);
        Serial.print(" tReady2="); Serial.print(timerReady2);
        Serial.print(" tempOk1="); Serial.print(tempOk1);
        Serial.print(" tempOk2="); Serial.print(tempOk2);
        Serial.print(" partner1="); Serial.print((currentTemp2 >= (float)allowedT2));
        Serial.print(" partner2="); Serial.println((currentTemp1 >= (float)allowedT1));
#endif

        bool incremented = false;
        if (need1 && need2) {
          if (timerReady1 && timerReady2 && tempOk1 && tempOk2) {
            allowedT1++; allowedT2++;
            eqHoldReady1 = eqHoldReady2 = false;
            eqHoldStart1 = eqHoldStart2 = now;
            incremented = true;
#ifdef DEBUG_VERBOSE
            Serial.print("[DBG] simultaneous increment -> allowedT1="); Serial.print(allowedT1);
            Serial.print(" allowedT2="); Serial.println(allowedT2);
#endif
          }
        } else if (need1 && !need2) {
          if (timerReady1 && tempOk1 && (currentTemp2 >= (float)allowedT2)) {
            allowedT1++;
            eqHoldReady1 = false;
            eqHoldStart1 = now;
            incremented = true;
#ifdef DEBUG_VERBOSE
            Serial.print("[DBG] single T1 increment -> allowedT1="); Serial.println(allowedT1);
#endif
          } else {
#ifdef DEBUG_VERBOSE
            Serial.print("[DBG] single T1 NOT incremented:");
            if (!timerReady1) Serial.print(" wait_timer");
            if (!tempOk1) Serial.print(" wait_temp");
            if (!(currentTemp2 >= (float)allowedT2)) Serial.print(" partner_not_reached");
            Serial.println();
#endif
          }
        } else if (!need1 && need2) {
          if (timerReady2 && tempOk2 && (currentTemp1 >= (float)allowedT1)) {
            allowedT2++;
            eqHoldReady2 = false;
            eqHoldStart2 = now;
            incremented = true;
#ifdef DEBUG_VERBOSE
            Serial.print("[DBG] single T2 increment -> allowedT2="); Serial.println(allowedT2);
#endif
          } else {
#ifdef DEBUG_VERBOSE
            Serial.print("[DBG] single T2 NOT incremented:");
            if (!timerReady2) Serial.print(" wait_timer");
            if (!tempOk2) Serial.print(" wait_temp");
            if (!(currentTemp1 >= (float)allowedT1)) Serial.print(" partner_not_reached");
            Serial.println();
#endif
          }
        }

        if (incremented) {
          updateStepsCompletedFromAllowed(now);
        }

        // Safety bounds (no decreases, just cap)
        int wantedDiff = abs(targetTemp1 - targetTemp2);
        if (allowedT1 > allowedT2 + wantedDiff) allowedT1 = allowedT2 + wantedDiff;
        if (allowedT2 > allowedT1 + wantedDiff) allowedT2 = allowedT1 + wantedDiff;
        if (allowedT1 > targetTemp1) allowedT1 = targetTemp1;
        if (allowedT2 > targetTemp2) allowedT2 = targetTemp2;
      }
    }
  }

  // HEAT: compute remain using shared logic (sync progress -> calcRemainTime)
  if (processActive && heatingPhase && !alignPhase) {
    updateStepsCompletedFromAllowed(now);

    if (lastStepTime == 0) {
      int s1 = allowedT1 - startTemp1 - 1; if (s1 < 0) s1 = 0;
      int s2 = allowedT2 - startTemp2 - 1; if (s2 < 0) s2 = 0;
      bool ch1Done = (allowedT1 >= targetTemp1);
      bool ch2Done = (allowedT2 >= targetTemp2);
      int newCompleted;
      if (!ch1Done && !ch2Done) newCompleted = min(s1, s2);
      else if (ch1Done && !ch2Done) newCompleted = s2;
      else if (!ch1Done && ch2Done) newCompleted = s1;
      else newCompleted = max(s1, s2);
      if (newCompleted < 0) newCompleted = 0;
      stepsCompleted = newCompleted;
      lastStepTime = now;
      lastCompletedGlobal = stepsCompleted;
      lastAllowedChangeTime = lastStepTime;
      displayVirtualConsumed = 0;
#ifdef DEBUG_VERBOSE
      Serial.print("[DBG-INIT] HEAT init lastStepTime="); Serial.print(lastStepTime);
      Serial.print(" stepsCompleted="); Serial.println(stepsCompleted);
#endif
    } else {
      lastAllowedChangeTime = lastStepTime;
      lastCompletedGlobal = stepsCompleted;
      displayVirtualConsumed = 0;
    }

    calcRemainTime();

#ifdef DEBUG_VERBOSE
    Serial.print("[DBG-LEFT] (via calcRemainTime) remainSeconds="); Serial.print(remainSeconds);
    Serial.print(" lastStepTime="); Serial.print(lastStepTime);
    Serial.print(" stepsCompleted="); Serial.print(stepsCompleted);
    Serial.print(" lastAllowedChangeTime="); Serial.print(lastAllowedChangeTime);
    Serial.print(" lastCompletedGlobal="); Serial.print(lastCompletedGlobal);
    Serial.print(" displayVirtualConsumed="); Serial.println(displayVirtualConsumed);
#endif

    if ((long)remainSeconds != lastDrawnRemainSeconds) {
      drawScreen(allowedT1, allowedT2);
      lastDrawnRemainSeconds = remainSeconds;
      lastScreenUpdate = now;
#ifdef DEBUG_VERBOSE
      Serial.print("[DBG-UI] forced redraw: remainSeconds="); Serial.println(remainSeconds);
#endif
    }
  } else {
    if ((long)remainSeconds != lastDrawnRemainSeconds && (millis() - lastScreenUpdate) >= 500) {
      drawScreen(allowedT1, allowedT2);
      lastDrawnRemainSeconds = remainSeconds;
      lastScreenUpdate = millis();
#ifdef DEBUG_VERBOSE
      Serial.print("[DBG-UI] redraw (non-HEAT): remainSeconds="); Serial.println(remainSeconds);
#endif
    }
  }

  // Transition to HOLD
  if (processActive && heatingPhase && !alignPhase) {
    bool timerDone = (remainSeconds == 0);

    bool allowedDone1 = (allowedT1 == targetTemp1);
    bool allowedDone2 = (allowedT2 == targetTemp2);

    // Дополнительная проверка: реальные температуры достигли таргетов
    bool tempReached1 = (!isnan(currentTemp1)) ? (currentTemp1 >= (float)targetTemp1) : false;
    bool tempReached2 = (!isnan(currentTemp2)) ? (currentTemp2 >= (float)targetTemp2) : false;

    bool done1 = allowedDone1 && tempReached1;
    bool done2 = allowedDone2 && tempReached2;

    if (done1 && done2 && timerDone) {
      heatingPhase = false; holdingPhase = true; alignPhase = false;
      holdingStartTime = millis();
      // reset these for HOLD
      lastAllowedChangeTime = 0; displayVirtualConsumed = 0;
      lastStepTime = 0;
      stepsCompleted = 0;
      lastDrawnRemainSeconds = -1;
      printEvent("Holding phase started");
    } else {
#ifdef DEBUG_VERBOSE
      if (!allowedDone1) {
        Serial.print("[DBG] HOLD wait: allowedT1 != target ("); Serial.print(allowedT1); Serial.print(" != "); Serial.print(targetTemp1); Serial.println(")");
      } else if (!tempReached1) {
        Serial.print("[DBG] HOLD wait: T1 not yet reached target (cur="); Serial.print(currentTemp1,2); Serial.print(" < target="); Serial.print(targetTemp1); Serial.println(")");
      }

      if (!allowedDone2) {
        Serial.print("[DBG] HOLD wait: allowedT2 != target ("); Serial.print(allowedT2); Serial.print(" != "); Serial.print(targetTemp2); Serial.println(")");
      } else if (!tempReached2) {
        Serial.print("[DBG] HOLD wait: T2 not yet reached target (cur="); Serial.print(currentTemp2,2); Serial.print(" < target="); Serial.print(targetTemp2); Serial.println(")");
      }

      if (allowedDone1 && allowedDone2 && !timerDone) {
        Serial.println("[DBG] HOLD wait: HEAT timer not finished (LEFT != 00:00)");
      }
#endif
    }
  }

  // SSR control (except ALIGN and AUTOTUNE)
  if (processActive && !alignPhase && !autotune1Active && !autotune2Active) {
    if (heatingPhase) controlSSRHeating();
    else if (holdingPhase) controlSSRHold();
    else {
      digitalWrite(HEATER1_PIN, LOW); digitalWrite(HEATER2_PIN, LOW);
      heater1State = heater2State = false;
    }
  }

  // Finish HOLD
  if (processActive && holdingPhase) {
    unsigned long nowMs = millis();
    if (nowMs - holdingStartTime >= (unsigned long)holdMinutes * ONE_MINUTE_MS) {
      holdingPhase = false; processActive = false; complete = true;
      digitalWrite(HEATER1_PIN, LOW); digitalWrite(HEATER2_PIN, LOW);
      heater1State = heater2State = false;
      printEvent("Process complete");
    }
  }

  if (processActive && alignPhase) {
    alignTemperatures(allowedT1, allowedT2);
    drawScreen(allowedT1, allowedT2);
    lastScreenUpdate = millis();
    if (heater1State != lastHeater1State || heater2State != lastHeater2State) {
      float diff1 = allowedT1 - currentTemp1;
      float diff2 = allowedT2 - currentTemp2;
      printVerboseDebug(
        allowedT1, allowedT2,
        currentTemp1, currentTemp2,
        diff1, diff2,
        Output1, Output2,
        heatingPhase, holdingPhase, processActive,
        heater1State, heater2State
      );
      lastHeater1State = heater1State;
      lastHeater2State = heater2State;
    }
    return;
  }

  // --- AUTOTUNE ---
  updateAutotune();

  if (now - lastScreenUpdate >= screenPeriod) {
    if (sensorsReady) drawScreen(allowedT1, allowedT2);
    else drawScreen(-1, -1);
    lastScreenUpdate = now;
  }

  if (heater1State != lastHeater1State || heater2State != lastHeater2State) {
    float diff1 = allowedT1 - currentTemp1;
    float diff2 = allowedT2 - currentTemp2;
    printVerboseDebug(
      allowedT1, allowedT2,
      currentTemp1, currentTemp2,
      diff1, diff2,
      Output1, Output2,
      heatingPhase, holdingPhase, processActive,
      heater1State, heater2State
    );
    lastHeater1State = heater1State;
    lastHeater2State = heater2State;
  }
  SIM_DELAY(100);
}