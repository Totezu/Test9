#include "Buttons.h"
#include "Utils.h"
#include "Autotune.h"
#include "Process.h"
#include "EepromStore.h"

void setupButtons() {
  for (int i = 0; i < 10; i++) pinMode(btnPins[i], INPUT);
  for (int i = 0; i < 10; i++) btnPrev[i] = digitalRead(btnPins[i]) == HIGH;
}

void boundParams() {
  if (targetTemp1 < 0) targetTemp1 = 0;
  if (targetTemp2 < 0) targetTemp2 = 0;
  if (targetTemp1 > 300) targetTemp1 = 300;
  if (targetTemp2 > 300) targetTemp2 = 300;
  if (holdMinutes < 1) holdMinutes = 1;
  if (holdMinutes > 999) holdMinutes = 999;
  if (temp1_offset < -30) temp1_offset = -30;
  if (temp1_offset > 30) temp1_offset = 30;
  if (temp2_offset < -30) temp2_offset = -30;
  if (temp2_offset > 30) temp2_offset = 30;
}

// ====== ЛОГИКА удержания для кнопок A0..A5 ======
static const uint8_t HOLD_BTN_COUNT = 6;

// Требуемое поведение:
// - первые initial_dead_ms ничего не происходит,
// - как только прошло initial_dead_ms — выполняется ПЕРВЫЙ шаг,
// - если удержание достигает repeatStartMs (1s) — сразу выполняется следующий шаг,
//   и затем повторяем шаги каждые repeatInterval (100 ms).
static const unsigned long initial_dead_ms = 50;   // игнор первых 300 ms
static const unsigned long repeatStartMs    = 1000;  // через 1s от начала удержания запускаем автоповтор
static const unsigned long repeatInterval   = 250;   // интервал автоповтора 100 ms

static unsigned long btnHoldStart[HOLD_BTN_COUNT] = {0};
static unsigned long btnLastStep[HOLD_BTN_COUNT] = {0};
static bool btnFirstFired[HOLD_BTN_COUNT] = {0};
static bool btnRepeatActive[HOLD_BTN_COUNT] = {0};

static inline void applyValueStepByIndex(uint8_t i) {
  if (!canChangeParams()) return;
  switch (i) {
    case 0: targetTemp1++; break;
    case 1: targetTemp1--; break;
    case 2: targetTemp2++; break;
    case 3: targetTemp2--; break;
    case 4: holdMinutes++; break;
    case 5: holdMinutes--; break;
  }
}

void handleButtonsAutoInc() {
  unsigned long now = millis();
  for (uint8_t i = 0; i < HOLD_BTN_COUNT; i++) {
    bool btnState = digitalRead(btnPins[i]) == HIGH;

    if (btnState && !btnPrev[i]) {
      // Начали удерживать: инициализируем состояния, не выполняем шаг сразу
      btnHoldStart[i] = now;
      btnLastStep[i] = 0;
      btnFirstFired[i] = false;
      btnRepeatActive[i] = false;
    }
    else if (btnState && btnPrev[i]) {
      unsigned long held = now - btnHoldStart[i];

      // До initial_dead_ms — ничего не делаем
      if (held < initial_dead_ms) {
        // noop
      } else {
        // Если первый шаг ещё не выполнен — выполняем его сразу при достижении initial_dead_ms
        if (!btnFirstFired[i]) {
          applyValueStepByIndex(i);
          btnFirstFired[i] = true;
          // btnLastStep оставляем 0 до момента, когда начнётся автоповтор
        }

        // Если достигнут порог автоповтора (repeatStartMs), включаем автоповтор.
        if (!btnRepeatActive[i] && held >= repeatStartMs) {
          // Выполняем дополнительный шаг сразу при запуске автоповтора
          applyValueStepByIndex(i);
          btnRepeatActive[i] = true;
          btnLastStep[i] = now;
        } else if (btnRepeatActive[i]) {
          // Выполняем повторные шаги с заданным интервалом
          if (now - btnLastStep[i] >= repeatInterval) {
            applyValueStepByIndex(i);
            btnLastStep[i] = now;
          }
        }
      }
    }
    else if (!btnState && btnPrev[i]) {
      // Отпустили кнопку — сбрасываем состояния удержания
      btnHoldStart[i] = 0;
      btnLastStep[i] = 0;
      btnFirstFired[i] = false;
      btnRepeatActive[i] = false;
    }

    btnPrev[i] = btnState;
  }
  boundParams();
}

void checkButtons() {
  bool btnState[10];
  for (int i = 6; i < 10; i++) btnState[i] = digitalRead(btnPins[i]) == HIGH;

  unsigned long now = millis();
  if (now - lastBtnTime > debounceDelay) {
    if (btnState[6] && !btnPrev[6] && !processActive && !autotune1Active && !autotune2Active) {
      if (sensorsReady && !isnan(currentTemp1) && !isnan(currentTemp2)) { startProcess(); printEvent("Button: START pressed"); }
      else printEvent("START ignored: sensors not ready");
      lastBtnTime = now;
    }
    if (btnState[7] && !btnPrev[7] && (processActive || autotune1Active || autotune2Active)) {
      stopAllProcesses(); lastBtnTime = now; printEvent("Button: STOP pressed");
    }
    if (btnState[8] && !btnPrev[8] && !autotune1Active && !processActive) {
      if (autotune1PendingSave) {
        Kp1 = autotuneKp1; Ki1 = autotuneKi1; Kd1 = autotuneKd1;
        pid1.SetTunings(Kp1, Ki1, Kd1);
        savePIDToEEPROM(Kp1, Ki1, Kd1, EEPROM_ADDR_KP1);
        autotune1PendingSave = false; targetTemp1 = savedTargetTemp1;
        printEvent("T1 autotune CONFIRMED and saved");
      } else { startAutotune1(); lastBtnTime = now; }
    }
    if (btnState[9] && !btnPrev[9] && !autotune2Active && !processActive) {
      if (autotune2PendingSave) {
        Kp2 = autotuneKp2; Ki2 = autotuneKi2; Kd2 = autotuneKd2;
        pid2.SetTunings(Kp2, Ki2, Kd2);
        savePIDToEEPROM(Kp2, Ki2, Kd2, EEPROM_ADDR_KP2);
        autotune2PendingSave = false; targetTemp2 = savedTargetTemp2;
        printEvent("T2 autotune CONFIRMED and saved");
      } else { startAutotune2(); lastBtnTime = now; }
    }
    if (btnState[7] && !btnPrev[7] && (autotune1PendingSave || autotune2PendingSave || autotune1Error || autotune2Error)) {
      if (autotune1PendingSave) { autotune1PendingSave = false; targetTemp1 = savedTargetTemp1; printEvent("T1 autotune result DISCARDED"); }
      else if (autotune2PendingSave) { autotune2PendingSave = false; targetTemp2 = savedTargetTemp2; printEvent("T2 autotune result DISCARDED"); }
      else if (autotune1Error) { autotune1Error = false; targetTemp1 = savedTargetTemp1; printEvent("T1 autotune error CLEARED"); }
      else if (autotune2Error) { autotune2Error = false; targetTemp2 = savedTargetTemp2; printEvent("T2 autotune error CLEARED"); }
    }
  }
  for (int i = 6; i < 10; i++) btnPrev[i] = btnState[i];
  boundParams();
}