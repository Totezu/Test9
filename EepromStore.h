#pragma once
#include "Config.h"
#include "Globals.h"
#include <Arduino.h>
#include <IPAddress.h>   // нужно для типа IPAddress

// -------------------------------
// One-time factory/version flags
// Выносим сюда все константы, чтобы их было легко менять.
// EEPROM_ADDR_FACTORY_VER  - адрес байта, где хранится версия "one-time clear"
// EEPROM_FACTORY_MAGIC     - опциональный magic-token (например 0xA5) — используйте при необходимости
// CURRENT_EEPROM_FACTORY_VER - текущая версия "one-time clear": увеличивайте при необходимости повторного выполнения
#ifndef EEPROM_ADDR_FACTORY_VER
  // выберите адрес вне используемых блоков; при необходимости измените
  #define EEPROM_ADDR_FACTORY_VER  524
#endif

#ifndef EEPROM_FACTORY_MAGIC
  // вспомогательный токен (пример 0xA5)
  #define EEPROM_FACTORY_MAGIC  0xA5
#endif

#ifndef CURRENT_EEPROM_FACTORY_VER
  // по умолчанию 1; при релизе новой прошивки, если нужно выполнить one-time clear снова,
  // увеличьте этот номер (например 2)
  #define CURRENT_EEPROM_FACTORY_VER  2
#endif

// Удобный alias/токен; значение записывается в EEPROM_ADDR_FACTORY_VER
#ifndef EEPROM_FACTORY_DONE_TOKEN
  #define EEPROM_FACTORY_DONE_TOKEN  ((uint8_t)CURRENT_EEPROM_FACTORY_VER)
#endif
// -------------------------------

// ---------- API ----------
void loadPIDFromEEPROM(double& Kp, double& Ki, double& Kd, int base, double defKp, double defKi, double defKd, const char* name);
void savePIDToEEPROM(double Kp, double Ki, double Kd, int base);

void saveTempOffsets();
void loadTempOffsets();

// config save/load (timings + targets)
void saveConfigToEEPROM();
void loadConfigFromEEPROM();

// Load persisted net config into provided refs.
// Note: IPs overwrite only if non-zero (0.0.0.0 ignored), to keep caller defaults.
void loadNetConfigFromEEPROM(bool& dhcp, IPAddress& ip, IPAddress& gw, IPAddress& sn, IPAddress& dns);

// Save provided net config to EEPROM.
void saveNetConfigToEEPROM(bool dhcp, const IPAddress& ip, const IPAddress& gw, const IPAddress& sn, const IPAddress& dns);

// One-time, versioned factory clear:
// При первом запуске новой прошивки performOneTimeEEPROMClear() запишет 0x00
// в нужные "magic" места (или другие адреса) и отметит в EEPROM, что операция выполнена.
// При следующей загрузке, если CURRENT_EEPROM_FACTORY_VER не изменился, операция не выполнится.
void performOneTimeEEPROMClear();

// Утилиты для полного/частичного сброса EEPROM (опционально)
// - factoryResetEEPROM(false)  -> soft: инвалидация магических меток (без полного вайпа)
// - factoryResetEEPROM(true)   -> full wipe: перезапись всего EEPROM 0xFF
void factoryResetEEPROM(bool fullWipe);