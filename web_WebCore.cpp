#include "web_WebInternals.h"
#include "Debug.h"
#ifdef __AVR__
  #include <avr/wdt.h>
#endif
#include <Ethernet.h> // для linkStatus() и begin(...) с таймаутами

// --- CS пины (проверьте под вашу плату/шилд)
#ifndef LAN_CS
  #define LAN_CS 10 // Arduino Ethernet Shield: W5100 CS = D10
#endif

// ---------- Глобальные переменные веб-части ----------
byte            web_mac[6]   = { 0xDE, 0xAD, 0xBE, 0x52, 0x39, 0x01 };
EthernetServer  web_server(80);
bool            web_serverStarted = false;

volatile uint32_t webUpdateSeq = 0;

bool      netDhcp = true;
IPAddress cfgIP(192,168,1,7);
IPAddress cfgGW(192,168,1,1);
IPAddress cfgSN(255,255,255,0);
IPAddress cfgDNS(192,168,1,1);

volatile bool         webRebootScheduled = false;
volatile unsigned long webRebootAtMs = 0;

// ---------- Асинхронная инициализация сети ----------
static bool web_init_in_progress = false;
static unsigned long web_init_last_try = 0;
static int  web_init_attempts = 0;

static const unsigned long WEB_DHCP_INTERVAL = 1000UL;
static const int           WEB_DHCP_MAX_ATTEMPTS = 3;

// Короткие таймауты DHCP (можно переопределить в Config.h)
#ifndef DHCP_TOTAL_TIMEOUT_MS
  #define DHCP_TOTAL_TIMEOUT_MS    3000UL
#endif
#ifndef DHCP_RESPONSE_TIMEOUT_MS
  #define DHCP_RESPONSE_TIMEOUT_MS 600UL
#endif

// Если макрос версии не подхватывается — можно форсировать короткий DHCP
#ifndef ETHERNET_VERSION
  #define ETHERNET_VERSION 0
#endif
#ifndef ETH_FORCE_SHORT_DHCP
  #define ETH_FORCE_SHORT_DHCP 0
#endif
#if (ETHERNET_VERSION >= 200) || ETH_FORCE_SHORT_DHCP
  #define ETH_HAS_SHORT_DHCP 1
#else
  #define ETH_HAS_SHORT_DHCP 0
#endif

// ---------- Helpers ----------
static inline bool link_is_off() {
#if defined(Ethernet_h) || defined(ETHERNET_H)
  return Ethernet.linkStatus() == LinkOFF;
#else
  return false;
#endif
}
static inline bool ip_invalid(const IPAddress& ip) {
  return (ip == IPAddress(0,0,0,0)) || (ip == IPAddress(255,255,255,255));
}

// ---------- Внутренняя маршрутизация ----------
static void route(EthernetClient& c, const String& reqLine) {
  int sp1 = reqLine.indexOf(' ');
  if (sp1<0) { sendHeader(c, "text/plain"); c.println(F("Bad request")); return; }
  int sp2 = reqLine.indexOf(' ', sp1+1); if (sp2<0) sp2=reqLine.length();
  String url = reqLine.substring(sp1+1, sp2);
  String path = url; String query;
  int q = url.indexOf('?'); if (q>=0) { path=url.substring(0,q); query=url.substring(q+1); }

  if (path == "/api/state")  { handleState(c); return; }
  if (path == "/api/digest") {
    sendHeader(c, "application/json; charset=utf-8");
    c.print('{');
    c.print(F("\"seq\":")); c.print(webUpdateSeq);
    c.print(F(",\"display\":"));  c.print(digest_display());
    c.print(F(",\"targets\":"));  c.print(digest_targets());
    c.print(F(",\"controls\":")); c.print(digest_controls());
    c.print(F(",\"offsets\":"));  c.print(digest_offsets());
    c.print(F(",\"timings\":"));  c.print(digest_timings());
    c.print(F(",\"pid1\":"));     c.print(digest_pid1());
    c.print(F(",\"pid2\":"));     c.print(digest_pid2());
    c.print(F(",\"autotune\":")); c.print(digest_autotune());
    c.print('}');
    return;
  }
  if (path == "/api/all")    { handleAll(c); return; }
  if (path == "/api/block")  { handleBlock(c, query); return; }
  if (path == "/api/set")    { handleSet(c, query); return; }
  if (path == "/api/save")   { handleSave(c, query); return; }
  if (path == "/")           { serveRoot(c); return; }

  sendHeader(c, "text/plain"); c.println(F("Not found"));
}

// ---------- Сеть ----------
static void ethernet_apply_static() {
#ifdef DEBUG_VERBOSE
  DBG_PRINTLN(F("[NET] Applying STATIC config"));
#endif
  Ethernet.begin(web_mac, cfgIP, cfgDNS, cfgGW, cfgSN);
  netDhcpSuccess = false;
}

static bool ethernet_try_dhcp_once() {
  // Если кабель точно OFF — не трогаем DHCP сейчас
  if (link_is_off()) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[NET] Link OFF — skip DHCP attempt"));
#endif
    return false;
  }

  int rc = 0;
#if ETH_HAS_SHORT_DHCP
#ifdef DEBUG_VERBOSE
  DBG_PRINT(F("[NET] DHCP begin (total=")); DBG_PRINT(DHCP_TOTAL_TIMEOUT_MS);
  DBG_PRINT(F("ms, resp=")); DBG_PRINT(DHCP_RESPONSE_TIMEOUT_MS); DBG_PRINTLN(F("ms)"));
#endif
  rc = Ethernet.begin(web_mac, (unsigned long)DHCP_TOTAL_TIMEOUT_MS, (unsigned long)DHCP_RESPONSE_TIMEOUT_MS);
#else
  rc = Ethernet.begin(web_mac); // может быть долгим, но с LinkOFF мы сюда не попадаем
#endif

  if (rc == 1) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[NET] DHCP OK"));
#endif
    netDhcpSuccess = true;
    return true;
  } else {
    web_init_attempts++;
#ifdef DEBUG_VERBOSE
    DBG_PRINT(F("[NET] DHCP failed #")); DBG_PRINTLN(web_init_attempts);
#endif
    if (web_init_attempts >= WEB_DHCP_MAX_ATTEMPTS) {
      netDhcpSuccess = false;
      return true; // исчерпали попытки -> в finalize перейдём на STATIC
    }
    return false; // попробуем ещё
  }
}

static void ethernet_start_finalize() {
  web_init_in_progress = false;

  if (!netDhcpSuccess) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[NET] DHCP failed/exhausted, falling back to STATIC"));
#endif
    ethernet_apply_static();
  }

  IPAddress curIP = Ethernet.localIP();
#ifdef DEBUG_VERBOSE
  DBG_PRINT(F("[NET] localIP now: ")); DBG_PRINTLN(ipToString(curIP));
#endif

  // Не запускаем сервер с некорректным IP
  if (ip_invalid(curIP)) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[NET] Invalid IP -> scheduling re-init"));
#endif
    web_init_in_progress = true;
    web_serverStarted = false;
    web_init_last_try = millis();
    web_init_attempts = 0;
    netDhcpSuccess = false;
    return;
  }

  web_server.begin();
  web_serverStarted = true;
#ifdef DEBUG_VERBOSE
  DBG_PRINT(F("[WEB] server started @ ")); DBG_PRINTLN(ipToString(Ethernet.localIP()));
#endif
}

// ----- Асинхронный старт -----
static void ethernet_start_async() {
  // На UNO/MEGA SS должен быть OUTPUT
  pinMode(SS, OUTPUT); digitalWrite(SS, HIGH);

  // Загружаем конфиг сети (если в EEPROM 0.0.0.0 — оставит значения из скетча)
  loadNetConfigFromEEPROM(netDhcp, cfgIP, cfgGW, cfgSN, cfgDNS);

  web_init_in_progress = true;
  web_init_last_try = millis();
  web_init_attempts = 0;
  netDhcpSuccess = false;

#ifdef DEBUG_VERBOSE
  DBG_PRINT(F("[NET] async start; DHCP=")); DBG_PRINTLN(netDhcp ? "ON" : "OFF");
#endif

  if (!netDhcp) {
    ethernet_apply_static();
    ethernet_start_finalize();
    return;
  }

  // Первая попытка DHCP (если не OFF)
  bool finished = ethernet_try_dhcp_once();
  if (finished) {
    ethernet_start_finalize();
    return;
  }
  // иначе — продолжим пытаться в webLoop()
}

// ---------- Публичный API ----------
void webSetup() {
  if (!webEnabled) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[WEB] webEnabled==false -> not starting Ethernet"));
#endif
    return;
  }
  if (web_serverStarted || web_init_in_progress) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[WEB] already started or init in progress"));
#endif
    return;
  }

#ifdef DEBUG_VERBOSE
  DBG_PRINTLN(F("[WEB] Ethernet init (async)..."));
#endif
  ethernet_start_async();
}

void webLoop() {
  if (!webEnabled) return;

  // Плановый ребут после сохранения сети
  if (webRebootScheduled && millis() >= webRebootAtMs) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[WEB] rebooting MCU to apply network config..."));
#endif
    delay(10);
    #ifdef __AVR__
      wdt_enable(WDTO_15MS);
      while(true) { }
    #else
      void(*resetFunc)(void) = 0;
      resetFunc();
      while(true) { }
    #endif
  }

  // Асинхронная инициализация/повторные попытки DHCP
  if (web_init_in_progress && !web_serverStarted) {
    unsigned long now = millis();
    if ((now - web_init_last_try) >= WEB_DHCP_INTERVAL) {
      web_init_last_try = now;

      // Если линк OFF — не дёргаем DHCP (не блокируем МК)
      if (link_is_off()) {
#ifdef DEBUG_VERBOSE
        DBG_PRINTLN(F("[NET] Link OFF — waiting for cable..."));
#endif
        return;
      }

      bool finished = ethernet_try_dhcp_once();
      if (finished) {
        ethernet_start_finalize();
      }
    }
    return;
  }

  if (!web_serverStarted) return;

  EthernetClient client = web_server.available();
  if (!client) return;

  // Первая строка запроса (короткий таймаут)
  String req; req.reserve(192);
  unsigned long t0 = millis();
  const unsigned long FIRST_LINE_TIMEOUT = 200UL;
  while (client.connected() && (millis()-t0) < FIRST_LINE_TIMEOUT) {
    if (client.available()) {
      char c = client.read();
      if (c == '\r') continue;
      if (c == '\n') break;
      if (req.length() < 192) req += c;
    }
  }
  if (req.length() == 0) { client.stop(); return; }

  // Заголовки: ждём пустую строку (короткий таймаут)
  int hdrLen = 0;
  unsigned long t1 = millis();
  const unsigned long HEADER_TIMEOUT = 300UL;
  while (client.connected() && (millis()-t1) < HEADER_TIMEOUT) {
    if (!client.available()) continue;
    char ch = client.read();
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (hdrLen == 0) break;
      hdrLen = 0;
    } else {
      if (hdrLen < 255) hdrLen++;
    }
  }

  route(client, req);
  client.stop();
}

void setWebEnabled(bool on) {
  if (webEnabled == on) return;
  webEnabled = on;
#ifdef DEBUG_VERBOSE
  DBG_PRINT(F("[WEB] setting webEnabled=")); DBG_PRINTLN(on ? "true" : "false");
#endif
  if (webEnabled) {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[WEB] enabling..."));
#endif
    webSetup();
  } else {
#ifdef DEBUG_VERBOSE
    DBG_PRINTLN(F("[WEB] disabling... (LAN CS will be held HIGH by deassertOtherSPI)"));
#endif
  }
}