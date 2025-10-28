#include "Control.h"

void controlSSRHeating() {
  unsigned long now = millis();

  if (now - windowStartTime1 >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime1) / windowSize;
    windowStartTime1 += windowsBehind * windowSize;
    Setpoint1 = (double)allowedT1 + PID_SETPOINT_OFFSET;
    if (Setpoint1 > targetTemp1) Setpoint1 = targetTemp1;
    Input1 = currentTemp1;
    pid1.SetOutputLimits(0, windowSize);
    pid1.Compute();
    pidOutput1 = Output1;
  }
  bool s1 = ((now - windowStartTime1) < pidOutput1) && (allowedT1 < targetTemp1);
  digitalWrite(HEATER1_PIN, s1 ? HIGH : LOW);
  heater1State = s1;

  if (now - windowStartTime2 >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime2) / windowSize;
    windowStartTime2 += windowsBehind * windowSize;
    Setpoint2 = (double)allowedT2 + PID_SETPOINT_OFFSET;
    if (Setpoint2 > targetTemp2) Setpoint2 = targetTemp2;
    Input2 = currentTemp2;
    pid2.SetOutputLimits(0, windowSize);
    pid2.Compute();
    pidOutput2 = Output2;
  }
  bool s2 = ((now - windowStartTime2) < pidOutput2) && (allowedT2 < targetTemp2);
  digitalWrite(HEATER2_PIN, s2 ? HIGH : LOW);
  heater2State = s2;
}

void controlSSRHold() {
  unsigned long now = millis();

  if (now - windowStartTime1 >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime1) / windowSize;
    windowStartTime1 += windowsBehind * windowSize;
    Setpoint1 = (double)targetTemp1;
    Input1 = currentTemp1;
    pid1.SetOutputLimits(0, windowSize);
    pid1.Compute();
    pidOutput1 = Output1;
  }
  bool s1 = ((now - windowStartTime1) < pidOutput1);
  digitalWrite(HEATER1_PIN, s1 ? HIGH : LOW);
  heater1State = s1;

  if (now - windowStartTime2 >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime2) / windowSize;
    windowStartTime2 += windowsBehind * windowSize;
    Setpoint2 = (double)targetTemp2;
    Input2 = currentTemp2;
    pid2.SetOutputLimits(0, windowSize);
    pid2.Compute();
    pidOutput2 = Output2;
  }
  bool s2 = ((now - windowStartTime2) < pidOutput2);
  digitalWrite(HEATER2_PIN, s2 ? HIGH : LOW);
  heater2State = s2;
}