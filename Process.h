#pragma once
#include "Config.h"
#include "Globals.h"

void startProcess();
void stopAllProcesses();
void alignTemperatures(int allowedT1_, int allowedT2_);
void calcRemainTime();