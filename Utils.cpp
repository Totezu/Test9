#include "Utils.h"

void printEvent(const char* msg) { Serial.println(msg); }
void printTempLog() {
  // Здесь можно реализовать реальное логирование в будущем
}

bool canChangeParams() {
  return (!autotune1Active && !autotune2Active) && (ALLOW_RUNTIME_PARAM_CHANGES || !processActive);
}

void printVerboseDebug(
    int /*allowedT1_*/, int /*allowedT2_*/,
    float currentT1_, float currentT2_,
    float diffT1_, float diffT2_,
    float out1_, float out2_,
    bool hPhase, bool holdPhase, bool pActive,
    bool relay1, bool relay2
) {
  char buf[192];
  char t1_str[8], t2_str[8], d1_str[8], d2_str[8], o1_str[8], o2_str[8];
  dtostrf(currentT1_, 6, 2, t1_str);
  dtostrf(currentT2_, 6, 2, t2_str);
  dtostrf(diffT1_, 6, 2, d1_str);
  dtostrf(diffT2_, 6, 2, d2_str);
  dtostrf(out1_, 6, 1, o1_str);
  dtostrf(out2_, 6, 1, o2_str);

  sprintf(buf,
    "[DEBUG] T1:%s T2:%s | d1=%s d2=%s | out1=%s out2=%s | h=%d hold=%d run=%d | H1=%s H2=%s",
    t1_str, t2_str, d1_str, d2_str, o1_str, o2_str,
    hPhase, holdPhase, pActive,
    relay1 ? "ON" : "OFF", relay2 ? "ON" : "OFF"
  );
  Serial.println(buf);
}

void formatRemain(unsigned long seconds, char *buf, size_t bufSize) {
  unsigned long mm = seconds / 60;
  unsigned long ss = seconds % 60;
  if (bufSize > 0) {
    if (mm < 100)
      snprintf(buf, bufSize, "%02lu:%02lu", mm, ss);
    else
      snprintf(buf, bufSize, "%lu:%02lu", mm, ss);
  }
}

void logTempsIfChanged() {
  int curT1 = (int)(currentTemp1 + 0.5);
  int curT2 = (int)(currentTemp2 + 0.5);
  if (lastLoggedTemp1 == -9999 && lastLoggedTemp2 == -9999) {
    printTempLog();
    lastLoggedTemp1 = curT1;
    lastLoggedTemp2 = curT2;
    lastLoggedTarget1 = targetTemp1;
    lastLoggedTarget2 = targetTemp2;
    return;
  }
  if (curT1 != lastLoggedTemp1 || curT2 != lastLoggedTemp2 ||
      targetTemp1 != lastLoggedTarget1 || targetTemp2 != lastLoggedTarget2) {
    printTempLog();
    lastLoggedTemp1 = curT1;
    lastLoggedTemp2 = curT2;
    lastLoggedTarget1 = targetTemp1;
    lastLoggedTarget2 = targetTemp2;
  }
}