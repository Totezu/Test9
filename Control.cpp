#include "Control.h"

// Helper: Update PID window and compute output for a single channel during heating phase
static void updateChannelHeating(
    unsigned long now,
    unsigned long& windowStartTime,
    double& Setpoint,
    int allowedTemp,
    int targetTemp,
    float currentTemp,
    double& Input,
    PID& pid,
    double& Output,
    double& pidOutput
) {
  if (now - windowStartTime >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime) / windowSize;
    windowStartTime += windowsBehind * windowSize;
    Setpoint = (double)allowedTemp + PID_SETPOINT_OFFSET;
    if (Setpoint > targetTemp) Setpoint = targetTemp;
    Input = currentTemp;
    pid.SetOutputLimits(0, windowSize);
    pid.Compute();
    pidOutput = Output;
  }
}

// Helper: Update PID window and compute output for a single channel during hold phase
static void updateChannelHold(
    unsigned long now,
    unsigned long& windowStartTime,
    double& Setpoint,
    int targetTemp,
    float currentTemp,
    double& Input,
    PID& pid,
    double& Output,
    double& pidOutput
) {
  if (now - windowStartTime >= windowSize) {
    unsigned long windowsBehind = (now - windowStartTime) / windowSize;
    windowStartTime += windowsBehind * windowSize;
    Setpoint = (double)targetTemp;
    Input = currentTemp;
    pid.SetOutputLimits(0, windowSize);
    pid.Compute();
    pidOutput = Output;
  }
}

void controlSSRHeating() {
  unsigned long now = millis();

  // Channel 1
  updateChannelHeating(now, windowStartTime1, Setpoint1, allowedT1, targetTemp1,
                       currentTemp1, Input1, pid1, Output1, pidOutput1);
  bool s1 = ((now - windowStartTime1) < pidOutput1) && (allowedT1 < targetTemp1);
  digitalWrite(HEATER1_PIN, s1 ? HIGH : LOW);
  heater1State = s1;

  // Channel 2
  updateChannelHeating(now, windowStartTime2, Setpoint2, allowedT2, targetTemp2,
                       currentTemp2, Input2, pid2, Output2, pidOutput2);
  bool s2 = ((now - windowStartTime2) < pidOutput2) && (allowedT2 < targetTemp2);
  digitalWrite(HEATER2_PIN, s2 ? HIGH : LOW);
  heater2State = s2;
}

void controlSSRHold() {
  unsigned long now = millis();

  // Channel 1
  updateChannelHold(now, windowStartTime1, Setpoint1, targetTemp1,
                    currentTemp1, Input1, pid1, Output1, pidOutput1);
  bool s1 = ((now - windowStartTime1) < pidOutput1);
  digitalWrite(HEATER1_PIN, s1 ? HIGH : LOW);
  heater1State = s1;

  // Channel 2
  updateChannelHold(now, windowStartTime2, Setpoint2, targetTemp2,
                    currentTemp2, Input2, pid2, Output2, pidOutput2);
  bool s2 = ((now - windowStartTime2) < pidOutput2);
  digitalWrite(HEATER2_PIN, s2 ? HIGH : LOW);
  heater2State = s2;
}