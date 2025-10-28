#include "EepromStore.h"
#include <EEPROM.h>
#include <Arduino.h>
#include <IPAddress.h>   // нужно для IPAddress
#include "Globals.h"
#include "Debug.h"

// default addresses (override in Config.h if needed)
#ifndef EEPROM_ADDR_T1_OFFSET
  #define EEPROM_ADDR_T1_OFFSET 200
#endif
#ifndef EEPROM_ADDR_T2_OFFSET
  #define EEPROM_ADDR_T2_OFFSET 204
#endif

// NEW: magic for offsets block (allows valid 0.0 values)
#ifndef EEPROM_ADDR_OFFSETS_MAGIC
  #define EEPROM_ADDR_OFFSETS_MAGIC 198
#endif
#ifndef EEPROM_OFFSETS_MAGIC_TOKEN
  #define EEPROM_OFFSETS_MAGIC_TOKEN 0xA5
#endif

#ifndef EEPROM_ADDR_CONFIG_MAGIC
  #define EEPROM_ADDR_CONFIG_MAGIC 400
#endif
#ifndef EEPROM_ADDR_ONE_MINUTE_MS
  #define EEPROM_ADDR_ONE_MINUTE_MS 404
#endif
#ifndef EEPROM_ADDR_AUTOTUNE_TIMEOUT
  #define EEPROM_ADDR_AUTOTUNE_TIMEOUT 408
#endif
#ifndef EEPROM_ADDR_TARGET1
  #define EEPROM_ADDR_TARGET1 412
#endif
#ifndef EEPROM_ADDR_TARGET2
  #define EEPROM_ADDR_TARGET2 416
#endif
#ifndef EEPROM_ADDR_HOLDMIN
  #define EEPROM_ADDR_HOLDMIN 420
#endif
#ifndef EEPROM_CONFIG_MAGIC_TOKEN
  #define EEPROM_CONFIG_MAGIC_TOKEN 0x5A
#endif

// ---------- Network config EEPROM (moved from WebInterface) ----------
// EEPROM addresses for network config and magic
#ifndef EEPROM_ADDR_NET_MAGIC
  #define EEPROM_ADDR_NET_MAGIC 500
#endif
#ifndef EEPROM_NET_MAGIC_TOKEN
  #define EEPROM_NET_MAGIC_TOKEN 0xC3
#endif
#ifndef EEPROM_ADDR_NET_DHCP
  #define EEPROM_ADDR_NET_DHCP (EEPROM_ADDR_NET_MAGIC+1)  // 1 byte
#endif
#ifndef EEPROM_ADDR_NET_IP
  #define EEPROM_ADDR_NET_IP   (EEPROM_ADDR_NET_MAGIC+4)  // 4 bytes
#endif
#ifndef EEPROM_ADDR_NET_GW
  #define EEPROM_ADDR_NET_GW   (EEPROM_ADDR_NET_IP+4)
#endif
#ifndef EEPROM_ADDR_NET_SN
  #define EEPROM_ADDR_NET_SN   (EEPROM_ADDR_NET_GW+4)
#endif
#ifndef EEPROM_ADDR_NET_DNS
  #define EEPROM_ADDR_NET_DNS  (EEPROM_ADDR_NET_SN+4)
#endif

// ---------- One-time factory/version flag ----------
#ifndef EEPROM_ADDR_FACTORY_VER
  // Choose an address outside used blocks (after net DNS at 516)
  #define EEPROM_ADDR_FACTORY_VER  524
#endif

// Current "factory clear" version. Increment this value when you want the
// one-time clear to run again on devices that already ran previous versions.
#ifndef CURRENT_EEPROM_FACTORY_VER
  #define CURRENT_EEPROM_FACTORY_VER  1
#endif

#ifndef EEPROM_FACTORY_DONE_TOKEN
  #define EEPROM_FACTORY_DONE_TOKEN  ((uint8_t)CURRENT_EEPROM_FACTORY_VER)
#endif

// ---------- helpers (local) ----------
static void eepromPutIP(int base, const IPAddress& ip) {
  for (uint8_t i = 0; i < 4; i++) EEPROM.update(base + i, ip[i]);
}
static void eepromGetIP(int base, IPAddress& ip) {
  uint8_t o[4]; for (uint8_t i=0;i<4;i++) o[i] = EEPROM.read(base + i);
  ip = IPAddress(o[0], o[1], o[2], o[3]);
}
static bool ipIsZero(const IPAddress& ip) {
  return ip[0]==0 && ip[1]==0 && ip[2]==0 && ip[3]==0;
}
static String ipToStr(const IPAddress& ip) {
  String s; s.reserve(16);
  s += String(ip[0]); s += '.'; s += String(ip[1]); s += '.';
  s += String(ip[2]); s += '.'; s += String(ip[3]);
  return s;
}

// ---------- PID load/save (unchanged logic, reduced prints) ----------
void loadPIDFromEEPROM(double& Kp, double& Ki, double& Kd, int base,
                       double defKp, double defKi, double defKd, const char* name) {
  double eKp = 0.0, eKi = 0.0, eKd = 0.0;
  int off = base;
  EEPROM.get(off, eKp); off += sizeof(double);
  EEPROM.get(off, eKi); off += sizeof(double);
  EEPROM.get(off, eKd);

  bool bad = false;
  if (isnan(eKp) || isnan(eKi) || isnan(eKd)) bad = true;
  if (!bad && eKp == 0.0 && eKi == 0.0 && eKd == 0.0) bad = true;

  if (bad) {
    Kp = defKp; Ki = defKi; Kd = defKd;
#ifdef DEBUG_VERBOSE
    DBG_PRINT("[PID] "); DBG_PRINT(name); DBG_PRINTLN(" - EEPROM empty/default, using sketch values");
#endif
  } else {
    Kp = eKp; Ki = eKi; Kd = eKd;
#ifdef DEBUG_VERBOSE
    DBG_PRINT("[PID] "); DBG_PRINT(name); DBG_PRINTLN(" - loaded from EEPROM");
#endif
  }

#ifdef DEBUG_VERBOSE
  DBG_PRINT("[PID] "); DBG_PRINT(name); DBG_PRINT(": Kp="); DBG_PRINT(Kp);
  DBG_PRINT(" Ki="); DBG_PRINT(Ki); DBG_PRINT(" Kd="); DBG_PRINTLN(Kd);
#endif
}

void savePIDToEEPROM(double Kp, double Ki, double Kd, int base) {
  int off = base;
  EEPROM.put(off, Kp); off += sizeof(double);
  EEPROM.put(off, Ki); off += sizeof(double);
  EEPROM.put(off, Kd);
#ifdef DEBUG_VERBOSE
  DBG_PRINTLN("[PID] saved to EEPROM");
#endif
}

// ---------- Offsets save/load ----------
void saveTempOffsets() {
  EEPROM.update(EEPROM_ADDR_OFFSETS_MAGIC, EEPROM_OFFSETS_MAGIC_TOKEN);
  EEPROM.put(EEPROM_ADDR_T1_OFFSET, temp1_offset);
  EEPROM.put(EEPROM_ADDR_T2_OFFSET, temp2_offset);
#ifdef DEBUG_VERBOSE
  DBG_PRINT("[EEPROM] temp offsets saved: temp1_offset=");
  DBG_PRINT(String(temp1_offset, 3));
  DBG_PRINT(" temp2_offset=");
  DBG_PRINTLN(String(temp2_offset, 3));
#endif
}

void loadTempOffsets() {
  uint8_t magic = EEPROM.read(EEPROM_ADDR_OFFSETS_MAGIC);
  if (magic != EEPROM_OFFSETS_MAGIC_TOKEN) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] temp offsets magic not found; keeping current offsets");
    DBG_PRINT("[EEPROM] current temp1_offset="); DBG_PRINTLN(String(temp1_offset, 3));
    DBG_PRINT("[EEPROM] current temp2_offset="); DBG_PRINTLN(String(temp2_offset, 3));
#endif
    return;
  }

  float e_t1 = 0.0f, e_t2 = 0.0f;
  EEPROM.get(EEPROM_ADDR_T1_OFFSET, e_t1);
  EEPROM.get(EEPROM_ADDR_T2_OFFSET, e_t2);

  bool t1Bad = isnan(e_t1) || (e_t1 < -100.0f || e_t1 > 100.0f);
  bool t2Bad = isnan(e_t2) || (e_t2 < -100.0f || e_t2 > 100.0f);

  if (t1Bad || t2Bad) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] temp offsets invalid/out of range; keeping current offsets");
    DBG_PRINT("[EEPROM] e_t1="); if (!isnan(e_t1)) { DBG_PRINT(String(e_t1, 3)); } else { DBG_PRINTLN("NaN"); }
    DBG_PRINT("[EEPROM] e_t2="); if (!isnan(e_t2)) { DBG_PRINTLN(String(e_t2, 3)); } else { DBG_PRINTLN("NaN"); }
#endif
    return;
  }

  temp1_offset = e_t1;
  temp2_offset = e_t2;
#ifdef DEBUG_VERBOSE
  DBG_PRINT("[EEPROM] temp offsets loaded: temp1_offset=");
  DBG_PRINT(String(temp1_offset, 3));
  DBG_PRINT(" temp2_offset=");
  DBG_PRINTLN(String(temp2_offset, 3));
#endif
}

// ---------- Runtime config (timings/targets) ----------
void saveConfigToEEPROM() {
  EEPROM.update(EEPROM_ADDR_CONFIG_MAGIC, EEPROM_CONFIG_MAGIC_TOKEN);
  EEPROM.put(EEPROM_ADDR_ONE_MINUTE_MS, one_minute_ms);
  EEPROM.put(EEPROM_ADDR_AUTOTUNE_TIMEOUT, autotune_timeout);
  EEPROM.put(EEPROM_ADDR_TARGET1, targetTemp1);
  EEPROM.put(EEPROM_ADDR_TARGET2, targetTemp2);
  EEPROM.put(EEPROM_ADDR_HOLDMIN, holdMinutes);
#ifdef DEBUG_VERBOSE
  DBG_PRINT("[EEPROM] config saved: one_minute_ms=");
  DBG_PRINT(one_minute_ms);
  DBG_PRINT(" autotune_timeout=");
  DBG_PRINT(autotune_timeout);
  DBG_PRINT(" target1=");
  DBG_PRINT(targetTemp1);
  DBG_PRINT(" target2=");
  DBG_PRINT(targetTemp2);
  DBG_PRINT(" holdMinutes=");
  DBG_PRINTLN(holdMinutes);
#endif
}

void loadConfigFromEEPROM() {
  uint8_t magic = EEPROM.read(EEPROM_ADDR_CONFIG_MAGIC);
  if (magic == EEPROM_CONFIG_MAGIC_TOKEN) {
    unsigned long v1 = 0, v2 = 0;
    int t1 = 0, t2 = 0, h = 0;
    EEPROM.get(EEPROM_ADDR_ONE_MINUTE_MS, v1);
    EEPROM.get(EEPROM_ADDR_AUTOTUNE_TIMEOUT, v2);
    EEPROM.get(EEPROM_ADDR_TARGET1, t1);
    EEPROM.get(EEPROM_ADDR_TARGET2, t2);
    EEPROM.get(EEPROM_ADDR_HOLDMIN, h);

    if (v1 >= 1000 && v1 < 3600000UL) one_minute_ms = v1;
#ifdef DEBUG_VERBOSE
    else { DBG_PRINTLN("[EEPROM] one_minute_ms out of range; using default"); }
#endif

    if (v2 >= 1000 && v2 < 86400000UL) autotune_timeout = v2;
#ifdef DEBUG_VERBOSE
    else { DBG_PRINTLN("[EEPROM] autotune_timeout out of range; using default"); }
#endif

    if (t1 >= 0 && t1 <= 400) targetTemp1 = t1;
    if (t2 >= 0 && t2 <= 400) targetTemp2 = t2;
    if (h >= 0 && h <= 1440) holdMinutes = h;

#ifdef DEBUG_VERBOSE
    DBG_PRINT("[EEPROM] config loaded: one_minute_ms=");
    DBG_PRINT(one_minute_ms);
    DBG_PRINT(" autotune_timeout=");
    DBG_PRINT(autotune_timeout);
    DBG_PRINT(" target1=");
    DBG_PRINT(targetTemp1);
    DBG_PRINT(" target2=");
    DBG_PRINT(targetTemp2);
    DBG_PRINT(" holdMinutes=");
    DBG_PRINTLN(holdMinutes);
#endif
  } else {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] config not found; using defaults");
#endif
  }
}

// ---------- Network config (moved from WebInterface) ----------
void loadNetConfigFromEEPROM(bool& dhcp, IPAddress& ip, IPAddress& gw, IPAddress& sn, IPAddress& dns) {
  uint8_t magic = EEPROM.read(EEPROM_ADDR_NET_MAGIC);
  if (magic != EEPROM_NET_MAGIC_TOKEN) {
    // keep defaults from caller
    return;
  }

  uint8_t dh = EEPROM.read(EEPROM_ADDR_NET_DHCP);
  IPAddress eip, egw, esn, edns;
  eepromGetIP(EEPROM_ADDR_NET_IP,  eip);
  eepromGetIP(EEPROM_ADDR_NET_GW,  egw);
  eepromGetIP(EEPROM_ADDR_NET_SN,  esn);
  eepromGetIP(EEPROM_ADDR_NET_DNS, edns);

  // If all stored addresses are 0.0.0.0 -> treat block as "empty" and keep sketch defaults
  if (ipIsZero(eip) && ipIsZero(egw) && ipIsZero(esn) && ipIsZero(edns)) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] network block contains 0.0.0.0 for all addresses -> using sketch defaults");
#endif
    return;
  }

  // Apply DHCP flag from EEPROM
  dhcp = (dh != 0);

  // Apply per-address only if EEPROM value is non-zero — else keep caller defaults
  if (!ipIsZero(eip))  ip  = eip;
  if (!ipIsZero(egw))  gw  = egw;
  if (!ipIsZero(esn))  sn  = esn;
  if (!ipIsZero(edns)) dns = edns;

#ifdef DEBUG_VERBOSE
  DBG_PRINTLN("[EEPROM] network config loaded from EEPROM (partial values preserved if 0.0.0.0)");
  DBG_PRINT(F("  dhcp=")); DBG_PRINTLN(dhcp ? "true" : "false");
  DBG_PRINT(F("  ip="));   DBG_PRINTLN(ipToStr(ip));
  DBG_PRINT(F("  gw="));   DBG_PRINTLN(ipToStr(gw));
  DBG_PRINT(F("  sn="));   DBG_PRINTLN(ipToStr(sn));
  DBG_PRINT(F("  dns="));  DBG_PRINTLN(ipToStr(dns));
#endif
}

void saveNetConfigToEEPROM(bool dhcp, const IPAddress& ip, const IPAddress& gw, const IPAddress& sn, const IPAddress& dns) {
  EEPROM.update(EEPROM_ADDR_NET_MAGIC, EEPROM_NET_MAGIC_TOKEN);
  EEPROM.update(EEPROM_ADDR_NET_DHCP, dhcp ? 1 : 0);
  eepromPutIP(EEPROM_ADDR_NET_IP,  ip);
  eepromPutIP(EEPROM_ADDR_NET_GW,  gw);
  eepromPutIP(EEPROM_ADDR_NET_SN,  sn);
  eepromPutIP(EEPROM_ADDR_NET_DNS, dns);
#ifdef DEBUG_VERBOSE
  DBG_PRINTLN("[EEPROM] network config saved");
#endif
}

// ---------- One-time, versioned factory clear ----------
void performOneTimeEEPROMClear() {
  // Read saved version from EEPROM
  uint8_t savedVer = EEPROM.read(EEPROM_ADDR_FACTORY_VER);
  if (savedVer == (uint8_t)CURRENT_EEPROM_FACTORY_VER) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] one-time clear already applied for this firmware version; skipping");
#endif
    return;
  }

#ifdef DEBUG_VERBOSE
  DBG_PRINT("[EEPROM] performing one-time factory clear (target version=");
  DBG_PRINT((int)CURRENT_EEPROM_FACTORY_VER);
  DBG_PRINTLN(")");
#endif

  EEPROM.update(EEPROM_ADDR_OFFSETS_MAGIC, 0x00);
  EEPROM.update(EEPROM_ADDR_CONFIG_MAGIC, 0x00);
  EEPROM.update(EEPROM_ADDR_NET_MAGIC, 0x00);

  {
    double z = 0.0;
    int off = EEPROM_ADDR_KP1;
    EEPROM.put(off + 0 * sizeof(double), z);
    EEPROM.put(off + 1 * sizeof(double), z);
    EEPROM.put(off + 2 * sizeof(double), z);
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] PID1 block zeroed");
#endif
  }
  {
    double z = 0.0;
    int off = EEPROM_ADDR_KP2;
    EEPROM.put(off + 0 * sizeof(double), z);
    EEPROM.put(off + 1 * sizeof(double), z);
    EEPROM.put(off + 2 * sizeof(double), z);
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN("[EEPROM] PID2 block zeroed");
#endif
  }

  EEPROM.update(EEPROM_ADDR_FACTORY_VER, (uint8_t)CURRENT_EEPROM_FACTORY_VER);

#if defined(ESP8266) || defined(ESP32)
  EEPROM.commit();
#endif

#ifdef DEBUG_VERBOSE
  DBG_PRINTLN("[EEPROM] one-time factory clear complete");
#endif
}