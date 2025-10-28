#include "web_WebInternals.h"

extern void setWebEnabled(bool on); // из WebCore.cpp (публичный API)

// ДОБАВЛЕНО: объявления функций из Process.h, чтобы не тянуть весь заголовок
extern void startProcess();
extern void stopAllProcesses();

// ---------- Apply ----------
bool applyParam(const String& k, const String& v) {
  auto keyEq=[&](const char* a)->bool{ return strEq(k,a); };

  // Уставки/timing
  if (keyEq("targetTemp1") || keyEq("t1")) { targetTemp1 = toIntSafe(v, targetTemp1); return true; }
  if (keyEq("targetTemp2") || keyEq("t2")) { targetTemp2 = toIntSafe(v, targetTemp2); return true; }
  if (keyEq("holdMinutes") || keyEq("hm")) { int m=toIntSafe(v, holdMinutes); if(m<0)m=0; holdMinutes=m; return true; }

  // PID1
  if (keyEq("Kp1") || keyEq("kp1")) { Kp1 = toDoubleSafe(v, Kp1); pid1.SetTunings(Kp1,Ki1,Kd1); return true; }
  if (keyEq("Ki1") || keyEq("ki1")) { Ki1 = toDoubleSafe(v, Ki1); pid1.SetTunings(Kp1,Ki1,Kd1); return true; }
  if (keyEq("Kd1") || keyEq("kd1")) { Kd1 = toDoubleSafe(v, Kd1); pid1.SetTunings(Kp1,Ki1,Kd1); return true; }

  // PID2
  if (keyEq("Kp2") || keyEq("kp2")) { Kp2 = toDoubleSafe(v, Kp2); pid2.SetTunings(Kp2,Ki2,Kd2); return true; }
  if (keyEq("Ki2") || keyEq("ki2")) { Ki2 = toDoubleSafe(v, Ki2); pid2.SetTunings(Kp2,Ki2,Kd2); return true; }
  if (keyEq("Kd2") || keyEq("kd2")) { Kd2 = toDoubleSafe(v, Kd2); pid2.SetTunings(Kp2,Ki2,Kd2); return true; }

  // Смещения
  if (keyEq("temp1_offset") || keyEq("o1")) { temp1_offset = toDoubleSafe(v, temp1_offset); return true; }
  if (keyEq("temp2_offset") || keyEq("o2")) { temp2_offset = toDoubleSafe(v, temp2_offset); return true; }

  // Флаги/режимы
  if (keyEq("sensorsDebug") || keyEq("sd")) {
    sensorsDebug = (v=="1" || v.equalsIgnoreCase("true") || v.equalsIgnoreCase("on"));
    return true;
  }

  if (keyEq("allowedT1")) { allowedT1 = toIntSafe(v, allowedT1); return true; }
  if (keyEq("allowedT2")) { allowedT2 = toIntSafe(v, allowedT2); return true; }

  if (keyEq("action") || keyEq("act")) {
    if (v.equalsIgnoreCase("start")) { startProcess(); return true; }
    if (v.equalsIgnoreCase("stop"))  { extern void stopAllProcesses(); stopAllProcesses(); return true; }
    return false;
  }

  if (keyEq("web") || keyEq("we")) {
    if (v.equalsIgnoreCase("on") || v=="1")  { setWebEnabled(true);  return true; }
    if (v.equalsIgnoreCase("off")|| v=="0")  { setWebEnabled(false); return true; }
    return false;
  }

  if (keyEq("autotune") || keyEq("au")) {
    if (v.equals("1")) { autotune1Active = true; autotune1Start = millis(); return true; }
    else if (v.equals("2")) { autotune2Active = true; autotune2Start = millis(); return true; }
    else if (v.equalsIgnoreCase("off")) { autotune1Active = autotune2Active = false; return true; }
    return false;
  }

  // Тайминги
  if (keyEq("one_minute_ms") || keyEq("om")) {
    unsigned long val = toULongSafe(v, one_minute_ms); if (val < 1000) val = 1000; one_minute_ms = val; return true;
  }
  if (keyEq("autotune_timeout") || keyEq("att")) {
    unsigned long val = toULongSafe(v, autotune_timeout); if (val < 1000) val = 1000; autotune_timeout = val; return true;
  }

  // Сеть: только локально применяем (персист по кнопке save?what=net)
  if (keyEq("ndhcp")) {
    netDhcp = (v=="1" || v.equalsIgnoreCase("true") || v.equalsIgnoreCase("on"));
    return true;
  }
  if (keyEq("nip"))  { IPAddress ip;  if (parseIP(v, ip))  { cfgIP  = ip;  return true; } return false; }
  if (keyEq("ngw"))  { IPAddress ip;  if (parseIP(v, ip))  { cfgGW  = ip;  return true; } return false; }
  if (keyEq("nsn"))  { IPAddress ip;  if (parseIP(v, ip))  { cfgSN  = ip;  return true; } return false; }
  if (keyEq("ndns")) { IPAddress ip;  if (parseIP(v, ip))  { cfgDNS = ip;  return true; } return false; }

  return false;
}

// Маппинг ключ -> блок
int8_t blockForKey(const String& k) {
  auto keyEq=[&](const char* a)->bool{ return strEq(k,a); };
  if (keyEq("targetTemp1")||keyEq("t1") || keyEq("targetTemp2")||keyEq("t2") || keyEq("holdMinutes")||keyEq("hm")) return BLK_TARGETS;
  if (keyEq("Kp1")||keyEq("kp1") || keyEq("Ki1")||keyEq("ki1") || keyEq("Kd1")||keyEq("kd1")) return BLK_PID1;
  if (keyEq("Kp2")||keyEq("kp2") || keyEq("Ki2")||keyEq("ki2") || keyEq("Kd2")||keyEq("kd2")) return BLK_PID2;
  if (keyEq("temp1_offset")||keyEq("o1") || keyEq("temp2_offset")||keyEq("o2")) return BLK_OFFSETS;
  if (keyEq("one_minute_ms")||keyEq("om") || keyEq("autotune_timeout")||keyEq("att")) return BLK_TIMINGS;
  if (keyEq("sensorsDebug")||keyEq("sd") || keyEq("web")||keyEq("we")) return BLK_CONTROLS;
  if (keyEq("allowedT1") || keyEq("allowedT2")) return BLK_DISPLAY;
  if (keyEq("action")||keyEq("act")) return BLK_DISPLAY;
  if (keyEq("autotune")||keyEq("au")) return BLK_AUTOTUNE;
  if (keyEq("ndhcp")||keyEq("nip")||keyEq("ngw")||keyEq("nsn")||keyEq("ndns")) return BLK_NET;
  return -1;
}

// ---------- HTTP handlers ----------
void handleState(EthernetClient& c) {
  sendHeader(c, "application/json; charset=utf-8");
  sendJsonStateBody(c);
}

void handleSet(EthernetClient& c, const String& query) {
  bool changed = false;
  bool blkChanged[BLK_COUNT]; for (uint8_t i=0;i<BLK_COUNT;++i) blkChanged[i]=false;

  int start=0;
  while (start < (int)query.length()) {
    int amp = query.indexOf('&', start); if (amp<0) amp=query.length();
    int eq = query.indexOf('=', start);
    if (eq>start && eq<amp) {
      String k = urlDecode(query.substring(start, eq));
      String v = urlDecode(query.substring(eq+1, amp));
      if (applyParam(k, v)) {
        changed = true;
        int8_t bid = blockForKey(k);
        if (bid>=0 && bid<BLK_COUNT) blkChanged[bid]=true;
        if (strEq(k,"action") || strEq(k,"act") || strEq(k,"autotune") || strEq(k,"au")) blkChanged[BLK_DISPLAY]=true;
        if (strEq(k,"web") || strEq(k,"we")) blkChanged[BLK_DISPLAY]=true;
      }
    }
    start = amp + 1;
  }

  if (changed) webUpdateSeq++;

  sendHeader(c, "application/json; charset=utf-8");
  c.print(F("{\"ok\":true,\"seq\":")); c.print(webUpdateSeq);

  bool any=false;
  for (uint8_t i=0;i<BLK_COUNT;++i) if (blkChanged[i]) { any=true; break; }
  if (any) {
    c.print(F(",\"blocks\":{"));
    bool first=true;
    auto sep=[&](void){ if (!first) c.print(','); first=false; };
    if (blkChanged[BLK_DISPLAY]) { sep(); c.print(F("\"display\":"));  sendJsonBlock_display(c); }
    if (blkChanged[BLK_TARGETS]) { sep(); c.print(F("\"targets\":"));  sendJsonBlock_targets(c); }
    if (blkChanged[BLK_CONTROLS]){ sep(); c.print(F("\"controls\":")); sendJsonBlock_controls(c); }
    if (blkChanged[BLK_OFFSETS]) { sep(); c.print(F("\"offsets\":"));  sendJsonBlock_offsets(c); }
    if (blkChanged[BLK_TIMINGS]) { sep(); c.print(F("\"timings\":"));  sendJsonBlock_timings(c); }
    if (blkChanged[BLK_PID1])    { sep(); c.print(F("\"pid1\":"));     sendJsonBlock_pid1(c); }
    if (blkChanged[BLK_PID2])    { sep(); c.print(F("\"pid2\":"));     sendJsonBlock_pid2(c); }
    if (blkChanged[BLK_AUTOTUNE]){ sep(); c.print(F("\"autotune\":")); sendJsonBlock_autotune(c); }
    if (blkChanged[BLK_NET])     { sep(); c.print(F("\"net\":"));      sendJsonBlock_net(c); }
    c.print('}');
  }

  c.println(F("}"));
}

void handleSave(EthernetClient& c, const String& query) {
  String what;
  int eq = query.indexOf('=');
  if (eq>=0) what = query.substring(eq+1);
  what.trim();

  if (what.equalsIgnoreCase("offsets")) {
    saveTempOffsets();
  } else if (what.equalsIgnoreCase("pid1")) {
    savePIDToEEPROM(Kp1, Ki1, Kd1, EEPROM_ADDR_KP1);
  } else if (what.equalsIgnoreCase("pid2")) {
    savePIDToEEPROM(Kp2, Ki2, Kd2, EEPROM_ADDR_KP2);
  } else if (what.equalsIgnoreCase("config")) {
    saveConfigToEEPROM();
  } else if (what.equalsIgnoreCase("all")) {
    saveTempOffsets();
    savePIDToEEPROM(Kp1, Ki1, Kd1, EEPROM_ADDR_KP1);
    savePIDToEEPROM(Kp2, Ki2, Kd2, EEPROM_ADDR_KP2);
    saveConfigToEEPROM();
  } else if (what.equalsIgnoreCase("net")) {
    // Persist + плановый ребут (сам ребут в webLoop)
    saveNetConfigToEEPROM(netDhcp, cfgIP, cfgGW, cfgSN, cfgDNS);
    webRebootScheduled = true;
    webRebootAtMs = millis() + 100;
  }

  webUpdateSeq++;

  sendHeader(c, "application/json; charset=utf-8");
  c.print(F("{\"ok\":true,\"seq\":"));
  c.print(webUpdateSeq);
  c.println(F("}"));
}

void handleAll(EthernetClient& c) {
  sendHeader(c, "application/json; charset=utf-8");
  c.print('{');
  c.print(F("\"seq\":")); c.print(webUpdateSeq);
  c.print(F(",\"display\":"));  sendJsonBlock_display(c);
  c.print(F(",\"targets\":"));  sendJsonBlock_targets(c);
  c.print(F(",\"controls\":")); sendJsonBlock_controls(c);
  c.print(F(",\"offsets\":"));  sendJsonBlock_offsets(c);
  c.print(F(",\"timings\":"));  sendJsonBlock_timings(c);
  c.print(F(",\"pid1\":"));     sendJsonBlock_pid1(c);
  c.print(F(",\"pid2\":"));     sendJsonBlock_pid2(c);
  c.print(F(",\"autotune\":")); sendJsonBlock_autotune(c);
  c.print(F(",\"net\":"));      sendJsonBlock_net(c);
  c.print('}');
}

void handleBlock(EthernetClient& c, const String& query) {
  String name;
  int start=0;
  while (start < (int)query.length()) {
    int amp = query.indexOf('&', start); if (amp<0) amp=query.length();
    int eq = query.indexOf('=', start);
    if (eq>start && eq<amp) {
      String k = urlDecode(query.substring(start, eq));
      if (k.equalsIgnoreCase("name")) {
        name = urlDecode(query.substring(eq+1, amp));
        break;
      }
    }
    start = amp + 1;
  }
  if (name.length()==0) {
    sendHeader(c, "application/json; charset=utf-8");
    c.println(F("{\"error\":\"missing name\"}"));
    return;
  }

  sendHeader(c, "application/json; charset=utf-8");
  if (name.equalsIgnoreCase("display")) { sendJsonBlock_display(c); return; }
  if (name.equalsIgnoreCase("targets")) { sendJsonBlock_targets(c); return; }
  if (name.equalsIgnoreCase("controls")){ sendJsonBlock_controls(c); return; }
  if (name.equalsIgnoreCase("offsets")) { sendJsonBlock_offsets(c); return; }
  if (name.equalsIgnoreCase("timings")) { sendJsonBlock_timings(c); return; }
  if (name.equalsIgnoreCase("pid1"))    { sendJsonBlock_pid1(c); return; }
  if (name.equalsIgnoreCase("pid2"))    { sendJsonBlock_pid2(c); return; }
  if (name.equalsIgnoreCase("autotune")){ sendJsonBlock_autotune(c); return; }
  if (name.equalsIgnoreCase("net"))     { sendJsonBlock_net(c); return; }

  c.println(F("{\"error\":\"unknown block\"}"));
}