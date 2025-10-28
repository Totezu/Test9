#pragma once
#include "Config.h"
#include "Globals.h"

bool beginMAX(Adafruit_MAX31856& dev);
void deassertOtherSPI();
void checkSensors();
void readTemperatures();

void setSensorsDebug(bool on);
void toggleSensorsDebug();