#include "Process.h"
#include "Sensors.h"
#include "Utils.h"
#include "Display.h"
#include "Control.h"

// --- Параметры подавления/защиты ---
static const unsigned long ALIGN_COMPLETE_SUPPRESS_MS = 1000UL;
static const unsigned long ALLOWED_SUPPRESS_MS = 1000UL;

// Процесс/фазы: stopAllProcesses, calcRemainTime, alignTemperatures, startProcess

void stopAllProcesses() {
  heatingPhase = false;
  holdingPhase = false;
  alignPhase = false;
  complete = false;
  processActive = false;
  remainSeconds = 0;

  digitalWrite(HEATER1_PIN, LOW);
  digitalWrite(HEATER2_PIN, LOW);
  heater1State = false;
  heater2State = false;

  eqHoldStart1 = 0;
  eqHoldStart2 = 0;

  if (autotune1Active) {
    autotune1Active = false;
    digitalWrite(HEATER1_PIN, LOW);
    if (heater1State) { heater1State = false; printEvent("Heater1 OFF (autotune stop)"); }
    printEvent("T1 autotune STOPPED");
    targetTemp1 = savedTargetTemp1;
  }
  if (autotune2Active) {
    autotune2Active = false;
    digitalWrite(HEATER2_PIN, LOW);
    if (heater2State) { heater2State = false; printEvent("Heater2 OFF (autotune stop)"); }
    printEvent("T2 autotune STOPPED");
    targetTemp2 = savedTargetTemp2;
  }

  if (tuner1 != nullptr) { delete tuner1; tuner1 = nullptr; }
  if (tuner2 != nullptr) { delete tuner2; tuner2 = nullptr; }

  lastCompletedGlobal = 0;
  lastAllowedChangeTime = 0;
  displayVirtualConsumed = 0;
  accumHoldMs1 = accumHoldMs2 = 0;
  lastLoopTs = 0;

#ifdef DEBUG_VERBOSE
  Serial.println("[DBG] stopAllProcesses: reset progress & accumulators");
#endif
  printEvent("Process/Autotune stopped");
}

void calcRemainTime() {
  if (!processActive) { remainSeconds = 0; return; }

  unsigned long now = millis();

  if (alignPhase) {
    // В ALIGN только считаем LEFT; allowed тут не изменяем
    int dTargetSigned = targetTemp1 - targetTemp2;
    int curDiff = allowedT1 - allowedT2;
    int wantedDiffAbs = abs(dTargetSigned);

    int remaining_steps = 0;
    if (curDiff == dTargetSigned) {
      remaining_steps = 0;
    } else if (curDiff < dTargetSigned) {
      // Нужно поднимать канал 1 (увеличивать allowedT1)
      int anchor2 = allowedT2; if (anchor2 > targetTemp2) anchor2 = targetTemp2;
      int goal1 = anchor2 + wantedDiffAbs; if (goal1 > targetTemp1) goal1 = targetTemp1;
      remaining_steps = goal1 - allowedT1;
    } else { // curDiff > dTargetSigned
      // Нужно поднимать канал 2 (увеличивать allowedT2)
      int anchor1 = allowedT1; if (anchor1 > targetTemp1) anchor1 = targetTemp1;
      int goal2 = anchor1 + wantedDiffAbs; if (goal2 > targetTemp2) goal2 = targetTemp2;
      remaining_steps = goal2 - allowedT2;
    }

    if (remaining_steps < 0) remaining_steps = 0;

    if (remaining_steps == 0) {
      remainSeconds = 0;
    } else {
      if (lastAllowedChangeTime != 0 && displayVirtualConsumed == 0) {
        unsigned long elapsed_ms = now - lastAllowedChangeTime;
        if (elapsed_ms >= ONE_MINUTE_MS) {
          displayVirtualConsumed = 1;
          lastAllowedChangeTime = 0;
        }
      }

      int displayedRemaining = remaining_steps - (int)displayVirtualConsumed;
      if (displayedRemaining < 0) displayedRemaining = 0;

      if (lastAllowedChangeTime == 0) {
        remainSeconds = (unsigned long)displayedRemaining * 60UL;
      } else {
        long long elapsed_ms = (long long)(now - lastAllowedChangeTime);
        long long total_rem_ms = (long long)displayedRemaining * (long long)ONE_MINUTE_MS;
        long long rem_ms = total_rem_ms - elapsed_ms;
        remainSeconds = (rem_ms <= 0) ? 0 : (unsigned long)((rem_ms * 60LL) / (long long)ONE_MINUTE_MS);
      }
    }
    return;
  }

  if (heatingPhase) {
    int t1 = (int)(targetTemp1 - startTemp1);
    int t2 = (int)(targetTemp2 - startTemp2);
    int maxStep = max(t1, t2);
    if (maxStep <= 0) { remainSeconds = 0; return; }

    int steps1 = allowedT1 - startTemp1 - 1; if (steps1 < 0) steps1 = 0;
    int steps2 = allowedT2 - startTemp2 - 1; if (steps2 < 0) steps2 = 0;

    bool ch1Done = (allowedT1 >= targetTemp1);
    bool ch2Done = (allowedT2 >= targetTemp2);
    int completedGlobal;
    if (!ch1Done && !ch2Done) completedGlobal = min(steps1, steps2);
    else if (ch1Done && !ch2Done) completedGlobal = steps2;
    else if (!ch1Done && ch2Done) completedGlobal = steps1;
    else completedGlobal = max(steps1, steps2);

    if (completedGlobal < 0) completedGlobal = 0;
    if (completedGlobal > maxStep) completedGlobal = maxStep;

    if (completedGlobal > lastCompletedGlobal) {
      lastCompletedGlobal = completedGlobal;
      lastAllowedChangeTime = now;
      displayVirtualConsumed = 0;
    } else {
      if (lastAllowedChangeTime != 0 && displayVirtualConsumed == 0) {
        unsigned long elapsed_ms = now - lastAllowedChangeTime;
        if (elapsed_ms >= ONE_MINUTE_MS) {
          displayVirtualConsumed = 1;
          lastAllowedChangeTime = 0;
        }
      }
    }

    int displayedCompleted = lastCompletedGlobal + (int)displayVirtualConsumed;
    if (displayedCompleted > maxStep) displayedCompleted = maxStep;
    int remaining_steps = maxStep - displayedCompleted;
    if (remaining_steps <= 0) {
      remainSeconds = 0;
    } else {
      if (lastAllowedChangeTime == 0) {
        remainSeconds = (unsigned long)remaining_steps * 60UL;
      } else {
        long long elapsed_ms = (long long)(now - lastAllowedChangeTime);
        long long total_rem_ms = (long long)remaining_steps * (long long)ONE_MINUTE_MS;
        long long rem_ms = total_rem_ms - elapsed_ms;
        remainSeconds = (rem_ms <= 0) ? 0 : (unsigned long)((rem_ms * 60LL) / (long long)ONE_MINUTE_MS);
      }
    }
  }
  else if (holdingPhase) {
    long long total_ms = (long long)holdMinutes * (long long)ONE_MINUTE_MS;
    long long elapsed_ms = (long long)(millis() - holdingStartTime);
    long long rem_ms = total_ms - elapsed_ms;
    remainSeconds = (rem_ms <= 0) ? 0 : (unsigned long)((rem_ms * 60LL) / (long long)ONE_MINUTE_MS);
  } else {
    remainSeconds = 0;
  }
}

void alignTemperatures(int /*allowedT1_*/, int /*allowedT2_*/) {
  unsigned long now = millis();

  // Выбираем направление по сравнению curDiff и dTarget (со знаком)
  int dTargetSigned = targetTemp1 - targetTemp2;
  int curDiff = allowedT1 - allowedT2;
  int wantedDiffAbs = abs(dTargetSigned);

  const int cur1r = (int)(currentTemp1 + 0.5);
  const int cur2r = (int)(currentTemp2 + 0.5);

#ifdef DEBUG_VERBOSE
  Serial.print("[DBG-ALIGN] enter: allowedT1="); Serial.print(allowedT1);
  Serial.print(" allowedT2="); Serial.print(allowedT2);
  Serial.print(" curDiff="); Serial.print(curDiff);
  Serial.print(" dTarget="); Serial.println(dTargetSigned);
#endif

  // Уже выровнено строго по знаку и величине
  if (curDiff == dTargetSigned) {
    alignPhase = false;
    heatingPhase = true;
    heatingStartTime = millis();
    heatEntryTime = heatingStartTime;
    lastAlignComplete = heatingStartTime;
    startTemp1 = cur1r;
    startTemp2 = cur2r;
    lastCompletedGlobal = 0;
    lastAllowedChangeTime = 0;
    displayVirtualConsumed = 0;

    unsigned long oneMinBefore = (heatingStartTime > ONE_MINUTE_MS) ? (heatingStartTime - ONE_MINUTE_MS) : heatingStartTime;
    if (allowedT1 < targetTemp1) eqHoldStart1 = (currentTemp1 >= (float)allowedT1) ? oneMinBefore : heatingStartTime; else eqHoldStart1 = 0;
    if (allowedT2 < targetTemp2) eqHoldStart2 = (currentTemp2 >= (float)allowedT2) ? oneMinBefore : heatingStartTime; else eqHoldStart2 = 0;

    digitalWrite(HEATER1_PIN, LOW); heater1State = false;
    digitalWrite(HEATER2_PIN, LOW); heater2State = false;
#ifdef DEBUG_VERBOSE
    Serial.println("[DBG][ALIGN] already aligned -> enter HEAT");
#endif
    printEvent("Alignment finished, starting stepwise heating");
    return;
  }

  if (curDiff < dTargetSigned) {
    // Нужно поднимать канал 1 (увеличивать allowedT1)
    int anchor2 = allowedT2; if (anchor2 > targetTemp2) anchor2 = targetTemp2;
    int goal1 = anchor2 + wantedDiffAbs; if (goal1 > targetTemp1) goal1 = targetTemp1;

    // PID: якорь канал 2 — держим около allowedT2
    if (now - windowStartTime2 >= windowSize) {
      unsigned long windowsBehind = (now - windowStartTime2) / windowSize;
      windowStartTime2 += windowsBehind * windowSize;
      double sp2 = (double)allowedT2 + PID_SETPOINT_OFFSET;
      if (sp2 > targetTemp2) sp2 = targetTemp2;
      Setpoint2 = sp2; Input2 = currentTemp2;
      pid2.SetOutputLimits(0, windowSize); pid2.Compute();
      pidOutput2 = Output2;
    }
    bool s2 = ((now - windowStartTime2) < pidOutput2);
    digitalWrite(HEATER2_PIN, s2 ? HIGH : LOW);
    heater2State = s2;

    // PID: тянущийся канал 1 — к goal1
    if (now - windowStartTime1 >= windowSize) {
      unsigned long windowsBehind = (now - windowStartTime1) / windowSize;
      windowStartTime1 += windowsBehind * windowSize;
      double sp1 = (double)allowedT1 + PID_SETPOINT_OFFSET;
      if (sp1 > goal1) sp1 = goal1;
      Setpoint1 = sp1; Input1 = currentTemp1;
      pid1.SetOutputLimits(0, windowSize); pid1.Compute();
      pidOutput1 = Output1;
    }
    bool s1 = ((now - windowStartTime1) < pidOutput1) && (allowedT1 < goal1);
    digitalWrite(HEATER1_PIN, s1 ? HIGH : LOW);
    heater1State = s1;

    // Шаги канала 1 (никогда не уменьшаем)
    if (eqHoldStart1 == 0) eqHoldStart1 = now;
    bool timerReady1 = (now - eqHoldStart1 >= ONE_MINUTE_MS);
    bool tempOk1    = (currentTemp1 >= (float)allowedT1);

    if (timerReady1 && tempOk1 && allowedT1 < goal1) {
      allowedT1++;
      if (allowedT1 > goal1) allowedT1 = goal1;
      if (allowedT1 > targetTemp1) allowedT1 = targetTemp1;

      eqHoldStart1 = now;
      lastAllowedUpdate1 = now;
      lastAllowedChangeTime = now;
      displayVirtualConsumed = 0;
#ifdef DEBUG_VERBOSE
      Serial.print("[DBG][ALIGN] T1 step -> allowedT1="); Serial.print(allowedT1);
      Serial.print(" (goal="); Serial.print(goal1); Serial.println(")");
#endif
    }

    // Завершение ALIGN
    curDiff = allowedT1 - allowedT2;
    if (curDiff >= dTargetSigned || allowedT1 >= goal1) {
      alignPhase = false;
      heatingPhase = true;
      heatingStartTime = millis();
      heatEntryTime = heatingStartTime;
      lastAlignComplete = heatingStartTime;
      startTemp1 = cur1r;
      startTemp2 = cur2r;
      lastCompletedGlobal = 0;
      lastAllowedChangeTime = 0;
      displayVirtualConsumed = 0;

      unsigned long oneMinBefore = (heatingStartTime > ONE_MINUTE_MS) ? (heatingStartTime - ONE_MINUTE_MS) : heatingStartTime;
      if (allowedT1 < targetTemp1) eqHoldStart1 = (currentTemp1 >= (float)allowedT1) ? oneMinBefore : heatingStartTime; else eqHoldStart1 = 0;
      if (allowedT2 < targetTemp2) eqHoldStart2 = (currentTemp2 >= (float)allowedT2) ? oneMinBefore : heatingStartTime; else eqHoldStart2 = 0;

      digitalWrite(HEATER1_PIN, LOW); heater1State = false;
      digitalWrite(HEATER2_PIN, LOW); heater2State = false;
#ifdef DEBUG_VERBOSE
      Serial.println("[DBG][ALIGN] done -> enter HEAT (raised T1)");
#endif
      printEvent("Alignment finished, starting stepwise heating");
    }

  } else { // curDiff > dTargetSigned
    // Нужно поднимать канал 2 (увеличивать allowedT2)
    int anchor1 = allowedT1; if (anchor1 > targetTemp1) anchor1 = targetTemp1;
    int goal2 = anchor1 + wantedDiffAbs; if (goal2 > targetTemp2) goal2 = targetTemp2;

    // PID: якорь канал 1
    if (now - windowStartTime1 >= windowSize) {
      unsigned long windowsBehind = (now - windowStartTime1) / windowSize;
      windowStartTime1 += windowsBehind * windowSize;
      double sp1 = (double)allowedT1 + PID_SETPOINT_OFFSET;
      if (sp1 > targetTemp1) sp1 = targetTemp1;
      Setpoint1 = sp1; Input1 = currentTemp1;
      pid1.SetOutputLimits(0, windowSize); pid1.Compute();
      pidOutput1 = Output1;
    }
    bool s1 = ((now - windowStartTime1) < pidOutput1);
    digitalWrite(HEATER1_PIN, s1 ? HIGH : LOW);
    heater1State = s1;

    // PID: тянущийся канал 2 — к goal2
    if (now - windowStartTime2 >= windowSize) {
      unsigned long windowsBehind = (now - windowStartTime2) / windowSize;
      windowStartTime2 += windowsBehind * windowSize;
      double sp2 = (double)allowedT2 + PID_SETPOINT_OFFSET;
      if (sp2 > goal2) sp2 = goal2;
      Setpoint2 = sp2; Input2 = currentTemp2;
      pid2.SetOutputLimits(0, windowSize); pid2.Compute();
      pidOutput2 = Output2;
    }
    bool s2 = ((now - windowStartTime2) < pidOutput2) && (allowedT2 < goal2);
    digitalWrite(HEATER2_PIN, s2 ? HIGH : LOW);
    heater2State = s2;

    // Шаги канала 2 (никогда не уменьшаем)
    if (eqHoldStart2 == 0) eqHoldStart2 = now;
    bool timerReady2 = (now - eqHoldStart2 >= ONE_MINUTE_MS);
    bool tempOk2    = (currentTemp2 >= (float)allowedT2);

    if (timerReady2 && tempOk2 && allowedT2 < goal2) {
      allowedT2++;
      if (allowedT2 > goal2) allowedT2 = goal2;
      if (allowedT2 > targetTemp2) allowedT2 = targetTemp2;

      eqHoldStart2 = now;
      lastAllowedUpdate2 = now;
      lastAllowedChangeTime = now;
      displayVirtualConsumed = 0;
#ifdef DEBUG_VERBOSE
      Serial.print("[DBG][ALIGN] T2 step -> allowedT2="); Serial.print(allowedT2);
      Serial.print(" (goal="); Serial.print(goal2); Serial.println(")");
#endif
    }

    // Завершение ALIGN
    curDiff = allowedT1 - allowedT2;
    if (curDiff <= dTargetSigned || allowedT2 >= goal2) {
      alignPhase = false;
      heatingPhase = true;
      heatingStartTime = millis();
      heatEntryTime = heatingStartTime;
      lastAlignComplete = heatingStartTime;
      startTemp1 = cur1r;
      startTemp2 = cur2r;
      lastCompletedGlobal = 0;
      lastAllowedChangeTime = 0;
      displayVirtualConsumed = 0;

      unsigned long oneMinBefore = (heatingStartTime > ONE_MINUTE_MS) ? (heatingStartTime - ONE_MINUTE_MS) : heatingStartTime;
      if (allowedT1 < targetTemp1) eqHoldStart1 = (currentTemp1 >= (float)allowedT1) ? oneMinBefore : heatingStartTime; else eqHoldStart1 = 0;
      if (allowedT2 < targetTemp2) eqHoldStart2 = (currentTemp2 >= (float)allowedT2) ? oneMinBefore : heatingStartTime; else eqHoldStart2 = 0;

      digitalWrite(HEATER1_PIN, LOW); heater1State = false;
      digitalWrite(HEATER2_PIN, LOW); heater2State = false;
#ifdef DEBUG_VERBOSE
      Serial.println("[DBG][ALIGN] done -> enter HEAT (raised T2)");
#endif
      printEvent("Alignment finished, starting stepwise heating");
    }
  }
}

void startProcess() {
  if (!sensorsReady || isnan(currentTemp1) || isnan(currentTemp2)) {
    printEvent("Cannot start: sensors not ready");
    return;
  }

  readTemperatures();
  startTemp1 = (int)(currentTemp1 + 0.5);
  startTemp2 = (int)(currentTemp2 + 0.5);

  // Инициализация allowed: стартовая +1
  allowedT1 = startTemp1 + 1;
  allowedT2 = startTemp2 + 1;
  lastAllowedUpdate1 = millis();
  lastAllowedUpdate2 = millis();

  unsigned long now = millis();
  eqHoldStart1 = now;
  eqHoldStart2 = now;

  accumHoldMs1 = accumHoldMs2 = 0;
  lastLoopTs = millis();

  // Новое правило выбора фазы: HEAT только когда разница и баланс шагов корректны
  int dAllowed0 = allowedT1 - allowedT2;
  int dTarget0  = targetTemp1 - targetTemp2;
  int left1 = (targetTemp1 > allowedT1) ? (targetTemp1 - allowedT1) : 0;
  int left2 = (targetTemp2 > allowedT2) ? (targetTemp2 - allowedT2) : 0;
  bool signedDiffOk  = (dAllowed0 == dTarget0);
  bool stepsBalanced = (left1 == left2);

  if (signedDiffOk && stepsBalanced) {
    alignPhase = false;
    heatingPhase = true;
    holdingPhase = false;
    printEvent("Stepwise heating (heatingPhase)");
  } else {
    alignPhase = true;
    heatingPhase = false;
    holdingPhase = false;
    printEvent("Temperature alignment (alignPhase)");
  }

  complete = false;
  processActive = true;
  remainSeconds = 0;
  heatingStartTime = millis();
  heatEntryTime = heatingStartTime;
  lastAlignComplete = 0;
  windowStartTime1 = millis();
  windowStartTime2 = millis();
  lastLoggedTemp1 = -9999;
  lastLoggedTemp2 = -9999;
  lastLoggedTarget1 = -9999;
  lastLoggedTarget2 = -9999;

  int init_steps1 = allowedT1 - startTemp1 - 1; if (init_steps1 < 0) init_steps1 = 0;
  int init_steps2 = allowedT2 - startTemp2 - 1; if (init_steps2 < 0) init_steps2 = 0;
  bool ch1Done = (allowedT1 >= targetTemp1);
  bool ch2Done = (allowedT2 >= targetTemp2);
  if (!ch1Done && !ch2Done) lastCompletedGlobal = min(init_steps1, init_steps2);
  else if (ch1Done && !ch2Done) lastCompletedGlobal = init_steps2;
  else if (!ch1Done && ch2Done) lastCompletedGlobal = init_steps1;
  else lastCompletedGlobal = max(init_steps1, init_steps2);

  lastAllowedChangeTime = heatingStartTime;
  displayVirtualConsumed = 0;

#ifdef DEBUG_VERBOSE
  Serial.print("[DBG] startProcess: startTemp1="); Serial.print(startTemp1);
  Serial.print(" startTemp2="); Serial.println(startTemp2);
  Serial.print("[DBG] startProcess: allowedT1="); Serial.print(allowedT1);
  Serial.print(" allowedT2="); Serial.println(allowedT2);
#endif
}