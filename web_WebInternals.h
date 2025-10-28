#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <IPAddress.h>
#include "Config.h"
#include "Globals.h"
#include "EepromStore.h"

// Глобалы веб-части (единственные определения в WebCore.cpp)
extern byte            web_mac[6];
extern EthernetServer  web_server;
extern bool            web_serverStarted;

// Последовательность обновлений для клиентов
extern volatile uint32_t webUpdateSeq;

// Сетевые параметры (редактируемые из UI)
extern bool      netDhcp;
extern IPAddress cfgIP, cfgGW, cfgSN, cfgDNS;

// Плановый ребут после сохранения сети
extern volatile bool         webRebootScheduled;
extern volatile unsigned long webRebootAtMs;

// ---------- Утилиты ----------
void sendHeader(EthernetClient& c, const char* ct = "text/html; charset=utf-8");
bool   strEq(const String& a, const char* b);
int    toIntSafe(const String& s, int def=0);
unsigned long toULongSafe(const String& s, unsigned long def=0UL);
double toDoubleSafe(const String& s, double def=0.0);
String urlDecode(const String& in);
bool   parseIP(const String& s, IPAddress& out);
String ipToString(const IPAddress& ip);
void   printBoolJSON(EthernetClient& c, bool v);
void   printDoubleJSON(EthernetClient& c, double v, uint8_t prec);

// ---------- Блоки состояния ----------
enum BlockId : uint8_t {
  BLK_DISPLAY=0,
  BLK_TARGETS,
  BLK_CONTROLS,
  BLK_OFFSETS,
  BLK_TIMINGS,
  BLK_PID1,
  BLK_PID2,
  BLK_AUTOTUNE,
  BLK_NET,
  BLK_COUNT
};

// Digest'ы (для старых клиентов)
uint32_t digest_display();
uint32_t digest_targets();
uint32_t digest_controls();
uint32_t digest_offsets();
uint32_t digest_timings();
uint32_t digest_pid1();
uint32_t digest_pid2();
uint32_t digest_autotune();

// JSON-блоки (streamed)
void sendJsonBlock_display(EthernetClient& c);
void sendJsonBlock_targets(EthernetClient& c);
void sendJsonBlock_controls(EthernetClient& c);
void sendJsonBlock_offsets(EthernetClient& c);
void sendJsonBlock_timings(EthernetClient& c);
void sendJsonBlock_pid1(EthernetClient& c);
void sendJsonBlock_pid2(EthernetClient& c);
void sendJsonBlock_autotune(EthernetClient& c);
void sendJsonBlock_net(EthernetClient& c);

// Legacy “/api/state”
void sendJsonStateBody(EthernetClient& c);

// ---------- Применение параметров + обработчики ----------
bool   applyParam(const String& k, const String& v);
int8_t blockForKey(const String& k);

// HTTP-обработчики
void handleState(EthernetClient& c);
void handleSet(EthernetClient& c, const String& query);
void handleSave(EthernetClient& c, const String& query);
void handleAll(EthernetClient& c);
void handleBlock(EthernetClient& c, const String& query);

// Страница
void serveRoot(EthernetClient& c);

// Инициализация сети
void ethernetStartWithConfig();