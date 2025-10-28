#include "web_WebInternals.h"
#include <string.h>

// ---------- FNV-1a ----------
static inline uint32_t fnv1a32_init() { return 2166136261UL; }
static inline uint32_t fnv1a32_update(uint32_t h, const uint8_t* p, size_t n) {
  for (size_t i=0;i<n;++i) { h ^= p[i]; h *= 16777619UL; }
  return h;
}
static inline uint32_t hash_bytes(uint32_t h, const void* data, size_t n) {
  return fnv1a32_update(h, (const uint8_t*)data, n);
}
static inline uint32_t hash_u32(uint32_t h, uint32_t v) { return hash_bytes(h, &v, sizeof(v)); }
static inline uint32_t hash_i32(uint32_t h, int32_t v) { return hash_bytes(h, &v, sizeof(v)); }
static inline uint32_t hash_bool(uint32_t h, bool v) { uint8_t b = v ? 1 : 0; return hash_bytes(h, &b, 1); }
static inline uint32_t hash_double(uint32_t h, double v) {
  uint8_t b[sizeof(double)]; memcpy(b, &v, sizeof(double));
  return fnv1a32_update(h, b, sizeof(double));
}

// ---------- Digests ----------
uint32_t digest_display() {
  uint32_t h = fnv1a32_init();
  h = hash_double(h, currentTemp1);
  h = hash_double(h, currentTemp2);
  h = hash_i32(h, allowedT1);
  h = hash_i32(h, allowedT2);
  h = hash_bool(h, processActive);
  h = hash_bool(h, heatingPhase);
  h = hash_bool(h, alignPhase);
  h = hash_bool(h, holdingPhase);
  h = hash_bool(h, complete);
  h = hash_bool(h, sensorsReady);
  h = hash_bool(h, heater1State);
  h = hash_bool(h, heater2State);
  h = hash_u32(h, remainSeconds);
  return h;
}
uint32_t digest_targets() {
  uint32_t h = fnv1a32_init();
  h = hash_i32(h, targetTemp1);
  h = hash_i32(h, targetTemp2);
  h = hash_i32(h, holdMinutes);
  return h;
}
uint32_t digest_controls() {
  uint32_t h = fnv1a32_init();
  h = hash_bool(h, sensorsDebug);
  h = hash_bool(h, webEnabled);
  return h;
}
uint32_t digest_offsets() {
  uint32_t h = fnv1a32_init();
  h = hash_double(h, temp1_offset);
  h = hash_double(h, temp2_offset);
  return h;
}
uint32_t digest_timings() {
  uint32_t h = fnv1a32_init();
  h = hash_u32(h, one_minute_ms);
  h = hash_u32(h, autotune_timeout);
  return h;
}
uint32_t digest_pid1() {
  uint32_t h = fnv1a32_init();
  h = hash_double(h, Kp1); h = hash_double(h, Ki1); h = hash_double(h, Kd1);
  return h;
}
uint32_t digest_pid2() {
  uint32_t h = fnv1a32_init();
  h = hash_double(h, Kp2); h = hash_double(h, Ki2); h = hash_double(h, Kd2);
  return h;
}
uint32_t digest_autotune() {
  uint32_t h = fnv1a32_init();
  h = hash_bool(h, autotune1Active); h = hash_bool(h, autotune2Active);
  h = hash_bool(h, autotune1PendingSave); h = hash_bool(h, autotune2PendingSave);
  h = hash_bool(h, autotune1Error); h = hash_bool(h, autotune2Error);
  h = hash_double(h, autotuneKp1); h = hash_double(h, autotuneKi1); h = hash_double(h, autotuneKd1);
  h = hash_double(h, autotuneKp2); h = hash_double(h, autotuneKi2); h = hash_double(h, autotuneKd2);
  return h;
}

// ---------- JSON blocks ----------
void sendJsonBlock_display(EthernetClient& c) {
  c.print('{');
  c.print(F("\"currentTemp1\":")); printDoubleJSON(c, currentTemp1, 2);
  c.print(F(",\"currentTemp2\":")); printDoubleJSON(c, currentTemp2, 2);
  c.print(F(",\"allowedT1\":"));   c.print(allowedT1);
  c.print(F(",\"allowedT2\":"));   c.print(allowedT2);
  c.print(F(",\"processActive\":")); printBoolJSON(c, processActive);
  c.print(F(",\"heatingPhase\":"));  printBoolJSON(c, heatingPhase);
  c.print(F(",\"alignPhase\":"));    printBoolJSON(c, alignPhase);
  c.print(F(",\"holdingPhase\":"));  printBoolJSON(c, holdingPhase);
  c.print(F(",\"complete\":"));      printBoolJSON(c, complete);
  c.print(F(",\"sensorsReady\":"));  printBoolJSON(c, sensorsReady);
  c.print(F(",\"heater1State\":"));  printBoolJSON(c, heater1State);
  c.print(F(",\"heater2State\":"));  printBoolJSON(c, heater2State);
  c.print(F(",\"remainSeconds\":")); c.print(remainSeconds);
  c.print('}');
}
void sendJsonBlock_targets(EthernetClient& c) {
  c.print('{');
  c.print(F("\"targetTemp1\":")); c.print(targetTemp1);
  c.print(F(",\"targetTemp2\":")); c.print(targetTemp2);
  c.print(F(",\"holdMinutes\":")); c.print(holdMinutes);
  c.print('}');
}
void sendJsonBlock_controls(EthernetClient& c) {
  c.print('{');
  c.print(F("\"sensorsDebug\":")); printBoolJSON(c, sensorsDebug);
  c.print(F(",\"webEnabled\":"));  printBoolJSON(c, webEnabled);
  c.print('}');
}
void sendJsonBlock_offsets(EthernetClient& c) {
  c.print('{');
  c.print(F("\"temp1_offset\":")); printDoubleJSON(c, temp1_offset, 3);
  c.print(F(",\"temp2_offset\":")); printDoubleJSON(c, temp2_offset, 3);
  c.print('}');
}
void sendJsonBlock_timings(EthernetClient& c) {
  c.print('{');
  c.print(F("\"one_minute_ms\":"));    c.print(one_minute_ms);
  c.print(F(",\"autotune_timeout\":")); c.print(autotune_timeout);
  c.print('}');
}
void sendJsonBlock_pid1(EthernetClient& c) {
  c.print('{');
  c.print(F("\"Kp1\":")); printDoubleJSON(c, Kp1, 2);
  c.print(F(",\"Ki1\":")); printDoubleJSON(c, Ki1, 2);
  c.print(F(",\"Kd1\":")); printDoubleJSON(c, Kd1, 2);
  c.print('}');
}
void sendJsonBlock_pid2(EthernetClient& c) {
  c.print('{');
  c.print(F("\"Kp2\":")); printDoubleJSON(c, Kp2, 2);
  c.print(F(",\"Ki2\":")); printDoubleJSON(c, Ki2, 2);
  c.print(F(",\"Kd2\":")); printDoubleJSON(c, Kd2, 2);
  c.print('}');
}
void sendJsonBlock_autotune(EthernetClient& c) {
  c.print('{');
  c.print(F("\"autotune1Active\":"));      printBoolJSON(c, autotune1Active);
  c.print(F(",\"autotune2Active\":"));      printBoolJSON(c, autotune2Active);
  c.print(F(",\"autotune1PendingSave\":")); printBoolJSON(c, autotune1PendingSave);
  c.print(F(",\"autotune2PendingSave\":")); printBoolJSON(c, autotune2PendingSave);
  c.print(F(",\"autotune1Error\":"));       printBoolJSON(c, autotune1Error);
  c.print(F(",\"autotune2Error\":"));       printBoolJSON(c, autotune2Error);
  c.print(F(",\"autotuneKp1\":")); printDoubleJSON(c, autotuneKp1, 2);
  c.print(F(",\"autotuneKi1\":")); printDoubleJSON(c, autotuneKi1, 2);
  c.print(F(",\"autotuneKd1\":")); printDoubleJSON(c, autotuneKd1, 2);
  c.print(F(",\"autotuneKp2\":")); printDoubleJSON(c, autotuneKp2, 2);
  c.print(F(",\"autotuneKi2\":")); printDoubleJSON(c, autotuneKi2, 2);
  c.print(F(",\"autotuneKd2\":")); printDoubleJSON(c, autotuneKd2, 2);
  c.print('}');
}
void sendJsonBlock_net(EthernetClient& c) {
  c.print('{');
  c.print(F("\"dhcp\":")); printBoolJSON(c, netDhcp);
  c.print(F(",\"ip\":\""));  c.print(ipToString(cfgIP));  c.print('"');
  c.print(F(",\"gw\":\""));  c.print(ipToString(cfgGW));  c.print('"');
  c.print(F(",\"sn\":\""));  c.print(ipToString(cfgSN));  c.print('"');
  c.print(F(",\"dns\":\"")); c.print(ipToString(cfgDNS)); c.print('"');
  c.print(F(",\"curIP\":\"")); c.print(ipToString(Ethernet.localIP())); c.print('"');
  c.print('}');
}

// ---------- Legacy /api/state ----------
void sendJsonStateBody(EthernetClient& c) {
  sendHeader(c, "application/json; charset=utf-8");
  c.print('{');

  c.print(F("\"currentTemp1\":")); printDoubleJSON(c, currentTemp1, 2);
  c.print(F(",\"currentTemp2\":")); printDoubleJSON(c, currentTemp2, 2);

  c.print(F(",\"targetTemp1\":")); c.print(targetTemp1);
  c.print(F(",\"targetTemp2\":")); c.print(targetTemp2);
  c.print(F(",\"allowedT1\":"));   c.print(allowedT1);
  c.print(F(",\"allowedT2\":"));   c.print(allowedT2);
  c.print(F(",\"holdMinutes\":")); c.print(holdMinutes);

  c.print(F(",\"Kp1\":")); printDoubleJSON(c, Kp1, 2);
  c.print(F(",\"Ki1\":")); printDoubleJSON(c, Ki1, 2);
  c.print(F(",\"Kd1\":")); printDoubleJSON(c, Kd1, 2);
  c.print(F(",\"Kp2\":")); printDoubleJSON(c, Kp2, 2);
  c.print(F(",\"Ki2\":")); printDoubleJSON(c, Ki2, 2);
  c.print(F(",\"Kd2\":")); printDoubleJSON(c, Kd2, 2);

  c.print(F(",\"temp1_offset\":")); printDoubleJSON(c, temp1_offset, 3);
  c.print(F(",\"temp2_offset\":")); printDoubleJSON(c, temp2_offset, 3);

  c.print(F(",\"processActive\":")); printBoolJSON(c, processActive);
  c.print(F(",\"heatingPhase\":"));  printBoolJSON(c, heatingPhase);
  c.print(F(",\"alignPhase\":"));    printBoolJSON(c, alignPhase);
  c.print(F(",\"holdingPhase\":"));  printBoolJSON(c, holdingPhase);
  c.print(F(",\"complete\":"));      printBoolJSON(c, complete);
  c.print(F(",\"sensorsReady\":"));  printBoolJSON(c, sensorsReady);

  c.print(F(",\"heater1State\":")); printBoolJSON(c, heater1State);
  c.print(F(",\"heater2State\":")); printBoolJSON(c, heater2State);

  c.print(F(",\"autotune1Active\":"));      printBoolJSON(c, autotune1Active);
  c.print(F(",\"autotune2Active\":"));      printBoolJSON(c, autotune2Active);
  c.print(F(",\"autotune1PendingSave\":")); printBoolJSON(c, autotune1PendingSave);
  c.print(F(",\"autotune2PendingSave\":")); printBoolJSON(c, autotune2PendingSave);
  c.print(F(",\"autotune1Error\":"));       printBoolJSON(c, autotune1Error);
  c.print(F(",\"autotune2Error\":"));       printBoolJSON(c, autotune2Error);
  c.print(F(",\"autotuneKp1\":")); printDoubleJSON(c, autotuneKp1, 2);
  c.print(F(",\"autotuneKi1\":")); printDoubleJSON(c, autotuneKi1, 2);
  c.print(F(",\"autotuneKd1\":")); printDoubleJSON(c, autotuneKd1, 2);
  c.print(F(",\"autotuneKp2\":")); printDoubleJSON(c, autotuneKp2, 2);
  c.print(F(",\"autotuneKi2\":")); printDoubleJSON(c, autotuneKi2, 2);
  c.print(F(",\"autotuneKd2\":")); printDoubleJSON(c, autotuneKd2, 2);

  c.print(F(",\"remainSeconds\":")); c.print(remainSeconds);
  c.print(F(",\"one_minute_ms\":")); c.print(one_minute_ms);
  c.print(F(",\"autotune_timeout\":")); c.print(autotune_timeout);

  c.print(F(",\"sensorsDebug\":")); printBoolJSON(c, sensorsDebug);
  c.print(F(",\"webEnabled\":"));   printBoolJSON(c, webEnabled);

  c.print('}');
}