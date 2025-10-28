#pragma once
#include <Arduino.h>

// ----------------- Основные флаги -----------------
#define USE_DISPLAY 1             // 0 — отключить дисплей (симулятор), 1 — включить (реальное устройство)
#define SIMULATION 0              // 1 — симулятор, 0 — железо
#define ALLOW_RUNTIME_PARAM_CHANGES 0 // 0 — менять параметры можно только когда процесс не идёт; 1 — разрешить менять в процессе (не во время автотюна)

// ----------------- SIM_DELAY -----------------
#if SIMULATION
  #define SIM_DELAY(x) delay(x)
#else
  #define SIM_DELAY(x)
#endif

#define ETH_FORCE_SHORT_DHCP 1

// ----------------- Библиотеки -----------------
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_MAX31856.h>
#include <PID_v1.h>
#include <PID_AutoTune_v0.h>
//#include <EEPROM.h>
#include <Controllino.h>

#if USE_DISPLAY
  #include <U8g2lib.h>
  // --- SSD1306 I2C OLED ---
  #define SCREEN_WIDTH 128
  #define SCREEN_HEIGHT 64
#endif

// ----------------- Тайминги и константы -----------------
#define DEFAULT_AUTOTUNE_TIMEOUT 1200000UL // default value for AUTOTUNE_TIMEOUT (20 minutes in ms)
#define DEFAULT_ONE_MINUTE_MS    10000UL   // default value for ONE_MINUTE_MS (10s for testing; 60000UL for real)

// Смещение точки нагрева для ступени PID
static const float PID_SETPOINT_OFFSET = 0.2;

// Размер окна для SSR
static const unsigned long windowSize = 4000;

// Мёртвая зона (на будущее, сейчас не используется)
static const float deadband = 0.01;

// ----------------- PID значения по умолчанию -----------------
static const double DEFAULT_KP1 = 800.0;
static const double DEFAULT_KI1 = 80.0;
static const double DEFAULT_KD1 = 10.0;
static const double DEFAULT_KP2 = 800.0;
static const double DEFAULT_KI2 = 80.0;
static const double DEFAULT_KD2 = 10.0;

// ----------------- MAX31856 SPI -----------------
#define MAX1_CS 42
#define MAX2_CS 44

// ----------------- Цифровые выходы (нагреватели) -----------------
#define HEATER1_PIN 2
#define HEATER2_PIN 3

// ----------------- Кнопки (внешние подтяжки, INPUT) -----------------
#define BTN1_PLUS A0
#define BTN1_MINUS A1
#define BTN2_PLUS A2
#define BTN2_MINUS A3
#define BTN_TIME_PLUS A4
#define BTN_TIME_MINUS A5
#define BTN_START A6
#define BTN_STOP A7
#define BTN1_AUTOTUNE A8
#define BTN2_AUTOTUNE A9

// ----------------- EEPROM адреса -----------------
#define EEPROM_ADDR_KP1 0
#define EEPROM_ADDR_KI1 8
#define EEPROM_ADDR_KD1 16
#define EEPROM_ADDR_KP2 24
#define EEPROM_ADDR_KI2 32
#define EEPROM_ADDR_KD2 40
#define EEPROM_ADDR_T1_OFFSET 48
#define EEPROM_ADDR_T2_OFFSET 52