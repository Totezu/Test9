#include "Display.h"
#include "Utils.h"       // for formatRemain
#include <Ethernet.h>    // for Ethernet.localIP()

#if USE_DISPLAY
// Helper: print current IP aligned to the right on the status line
static void drawRightAlignedIP() {
  IPAddress ip = Ethernet.localIP();
  char buf[32];
  snprintf(buf, sizeof(buf), "IP:%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

  // Make sure font is already selected before measuring width
  int w = display.getUTF8Width(buf);
  int x = SCREEN_WIDTH - w;
  if (x < 0) x = 0;

  // Status line baseline (same as below)
  const int y = 58;
  display.setCursor(x, y);
  display.print(buf);
}

// Helper: print connection type (DHCP or STATIC) aligned to the right on LEFT line
static void drawRightAlignedConnType() {
  const char* label = netDhcpSuccess ? "DHCP" : "STATIC";
  int w = display.getUTF8Width(label);
  int x = SCREEN_WIDTH - w;
  if (x < 0) x = 0;
  const int y = 46; // LEFT line baseline
  display.setCursor(x, y);
  display.print(label);
}

// Helper: draw a temperature line with all status indicators
static void drawTempLine(int y, const char* label, float current, int allowed, int target,
                         bool heaterOn, bool autotuneOn, bool alignOn) {
  display.setCursor(0, y);
  display.print(label);
  display.print(F(" "));
  if (isnan(current)) display.print(F("--"));
  else display.print((int)(current + 0.5));
  display.print(F("/"));
  if (allowed < 0) display.print(F("---")); else display.print(allowed);
  display.print(F("/"));
  display.print(target);
  if (heaterOn)    display.print(F(" H"));
  if (autotuneOn)  display.print(F(" A"));
  if (alignOn)     display.print(F(" ="));
}

// Рисуем содержимое одной «страницы» буфера
static void drawScreenContent(int allowedT1_, int allowedT2_) {
  display.setFont(u8g2_font_6x12_tr);

  // T1
  drawTempLine(10, "T1", currentTemp1, allowedT1_, targetTemp1, heater1State, autotune1Active, alignPhase);

  // T2
  drawTempLine(22, "T2", currentTemp2, allowedT2_, targetTemp2, heater2State, autotune2Active, alignPhase);

  // HOLD
  display.setCursor(0, 34);
  display.print(F("HOLD: "));
  display.print(holdMinutes);
  display.print(F("m"));

  // LEFT
  display.setCursor(0, 46);
  display.print(F("LEFT: "));
  if (remainSeconds > 0) {
    char timeBuf[16];
    formatRemain(remainSeconds, timeBuf, sizeof(timeBuf));
    display.print(timeBuf);
  } else {
    display.print(F("--:--"));
  }
  // Right-aligned connection type label (DHCP/STATIC)
  drawRightAlignedConnType();

  // STATUS + right-aligned IP (only for STOP/HEAT/ALIGN)
  const int statusY = 58;
  display.setCursor(0, statusY);
  if (allowedT1_ < 0 || allowedT2_ < 0) {
    display.print(F("WAIT SENSORS"));
  } else {
    bool showIP = false;
    if (autotune1Error) {
      display.print(F("T1:Error"));
      if (autotune2Error) display.print(F(" T2wait"));
    }
    else if (autotune2Error) {
      display.print(F("T2:Error"));
      if (autotune1Error) display.print(F(" T1wait"));
    }
    else if (autotune1PendingSave) {
      display.print(F("T1:"));
      display.print((int)(autotuneKp1 + 0.5)); display.print('/');
      display.print((int)(autotuneKi1 + 0.5)); display.print('/');
      display.print((int)(autotuneKd1 + 0.5)); display.print('?');
      if (autotune2PendingSave) display.print(F(" T2wait"));
    }
    else if (autotune2PendingSave) {
      display.print(F("T2:"));
      display.print((int)(autotuneKp2 + 0.5)); display.print('/');
      display.print((int)(autotuneKi2 + 0.5)); display.print('/');
      display.print((int)(autotuneKd2 + 0.5)); display.print('?');
      if (autotune1PendingSave) display.print(F(" T1wait"));
    }
    else if (complete) {
      display.print(F("DONE"));
    }
    else if (processActive) {
      if (alignPhase)        { display.print(F("ALIGN")); showIP = true; }
      else if (heatingPhase) { display.print(F("HEAT"));  showIP = true; }
      else if (holdingPhase) { display.print(F("HOLD"));  /* no IP */    }
      else                   { display.print(F("RUN"));   /* no IP */    }
    } else {
      display.print(F("STOP"));
      showIP = true;
    }

    if (showIP) {
      drawRightAlignedIP();
    }
  }
}
#endif

void drawScreen(int allowedT1_, int allowedT2_) {
#if USE_DISPLAY
  // Универсальный вариант, который работает и в full-buffer, и в page-buffer режимах U8g2
  display.firstPage();
  do {
    drawScreenContent(allowedT1_, allowedT2_);
  } while (display.nextPage());
#else
  (void)allowedT1_; (void)allowedT2_;
#endif
}