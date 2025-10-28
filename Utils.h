#pragma once
#include "Config.h"
#include "Globals.h"

void printEvent(const char* msg);
void printTempLog();

// Разрешены ли изменения параметров в текущем состоянии
bool canChangeParams();

// Подробный отладочный вывод
void printVerboseDebug(
    int allowedT1_, int allowedT2_,
    float currentT1_, float currentT2_,
    float diffT1_, float diffT2_,
    float out1_, float out2_,
    bool hPhase, bool holdPhase, bool pActive,
    bool relay1, bool relay2
);

// Форматирование времени (секунды -> MM:SS)
void formatRemain(unsigned long seconds, char *buf, size_t bufSize);

// Логировать температуры при изменении
void logTempsIfChanged();