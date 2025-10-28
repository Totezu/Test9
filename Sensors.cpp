#include "Sensors.h"
#include "Utils.h"
#include "Globals.h"
#include "Debug.h" // <-- добавлено
#include <SPI.h>

void deassertOtherSPI() {
  #ifdef MAX1_CS
  pinMode(MAX1_CS, OUTPUT);
  digitalWrite(MAX1_CS, HIGH);
  #endif
  #ifdef MAX2_CS
  pinMode(MAX2_CS, OUTPUT);
  digitalWrite(MAX2_CS, HIGH);
  #endif
  pinMode(SS, OUTPUT);
  digitalWrite(SS, HIGH);
  DDRJ |= _BV(PJ2); PORTJ |= _BV(PJ2); // RTC CS (D63) HIGH
  if (!webEnabled) { DDRJ |= _BV(PJ3); PORTJ |= _BV(PJ3); } // LAN CS HIGH
}

bool beginMAX(Adafruit_MAX31856& dev) {
  deassertOtherSPI();
  if (!dev.begin()) return false;
  dev.setThermocoupleType(MAX31856_TCTYPE_K);
  dev.setNoiseFilter(MAX31856_NOISE_FILTER_50HZ);
  delay(5);
  return true;
}

// Быстрый низкоуровневый probe регистра по CS (не блокирует высокоуровневые вызовы).
static uint8_t quickProbeByCS(uint8_t csPin, uint8_t reg) {
  pinMode(csPin, OUTPUT);
  digitalWrite(csPin, HIGH);
  deassertOtherSPI();
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE1));
  digitalWrite(csPin, LOW);
  uint8_t addr = reg & 0x7F;
  SPI.transfer(addr);
  uint8_t v = SPI.transfer(0x00);
  digitalWrite(csPin, HIGH);
  SPI.endTransaction();
  return v;
}

// Унифицированное поведение: при любом событии WAIT SENSORS — останавливаем все процессы.
static void enforceWaitSensorsAndStop(const char * /*reason*/) {
  allowedT1 = -1;
  allowedT2 = -1;
  sensorsReady = false;

  extern void stopAllProcesses();
  stopAllProcesses();

  processActive = false;
  heatingPhase = false;
  holdingPhase = false;
  alignPhase = false;

  autotune1Active = false;
  autotune2Active = false;
  autotune1PendingSave = false;
  autotune2PendingSave = false;
  autotune1Error = false;
  autotune2Error = false;
}

void checkSensors() {
  bool prevReady = sensorsReady;

  #ifdef MAX1_CS
  if (!max1Found) {
    uint8_t p = quickProbeByCS(MAX1_CS, MAX31856_CR1_REG);
    if (p == 0xFF || p == 0x00) {
      max1Found = false;
    } else {
      max1Found = beginMAX(max1);
      if (max1Found) max1ErrorCount = 0;
    }
  } else {
    uint8_t p = quickProbeByCS(MAX1_CS, MAX31856_CR1_REG);
    if (p == 0xFF) {
      max1ErrorCount = 3;
      max1Found = false;
    } else if (p == 0x00) {
      max1ErrorCount = 3;
      max1Found = false;
      enforceWaitSensorsAndStop("MAX1 CR1==0");
      return;
    }
  }
  #endif

  #ifdef MAX2_CS
  if (!max2Found) {
    uint8_t p = quickProbeByCS(MAX2_CS, MAX31856_CR1_REG);
    if (p == 0xFF || p == 0x00) {
      max2Found = false;
    } else {
      max2Found = beginMAX(max2);
      if (max2Found) max2ErrorCount = 0;
    }
  } else {
    uint8_t p = quickProbeByCS(MAX2_CS, MAX31856_CR1_REG);
    if (p == 0xFF) {
      max2ErrorCount = 3;
      max2Found = false;
    } else if (p == 0x00) {
      max2ErrorCount = 3;
      max2Found = false;
      enforceWaitSensorsAndStop("MAX2 CR1==0");
      return;
    }
  }
  #endif

  sensorsReady = max1Found && max2Found;

  if (!sensorsReady) {
    allowedT1 = -1;
    allowedT2 = -1;
    enforceWaitSensorsAndStop("sensor missing or uninitialized");
    return;
  }

  if (!prevReady && sensorsReady) {
    readTemperatures();
    startTemp1 = (int)(currentTemp1 + 0.5);
    startTemp2 = (int)(currentTemp2 + 0.5);
    allowedT1 = startTemp1 + 1;
    allowedT2 = startTemp2 + 1;
    lastAllowedUpdate1 = millis();
    lastAllowedUpdate2 = millis();
    eqHoldStart1 = eqHoldStart2 = 0;
  }
}

void readTemperatures() {
  deassertOtherSPI();

  float raw1 = NAN;
  float raw2 = NAN;

  #ifdef MAX1_CS
  if (max1Found) {
    uint8_t cr1 = quickProbeByCS(MAX1_CS, MAX31856_CR1_REG);
    if (cr1 == 0xFF) {
      max1ErrorCount = 3;
      max1Found = false;
      enforceWaitSensorsAndStop("MAX1 CR1==0xFF");
      return;
    }
    if (cr1 == 0x00) {
      max1ErrorCount = 3;
      max1Found = false;
      enforceWaitSensorsAndStop("MAX1 CR1==0x00");
      return;
    }

    raw1 = max1.readThermocoupleTemperature();
    uint8_t f1 = max1.readFault();
    bool bad1 = (f1 != 0) || isnan(raw1) || raw1 < -100.0f || raw1 > 1000.0f;
    if (bad1) {
      if (++max1ErrorCount >= 3) {
        max1Found = false;
        enforceWaitSensorsAndStop("MAX1 read BAD x3");
        return;
      }
    } else {
      max1ErrorCount = 0;
      lastGoodTemp1 = raw1;
    }
  }
  #endif

  #ifdef MAX2_CS
  if (max2Found) {
    uint8_t cr1 = quickProbeByCS(MAX2_CS, MAX31856_CR1_REG);
    if (cr1 == 0xFF) {
      max2ErrorCount = 3;
      max2Found = false;
      enforceWaitSensorsAndStop("MAX2 CR1==0xFF");
      return;
    }
    if (cr1 == 0x00) {
      max2ErrorCount = 3;
      max2Found = false;
      enforceWaitSensorsAndStop("MAX2 CR1==0x00");
      return;
    }

    raw2 = max2.readThermocoupleTemperature();
    uint8_t f2 = max2.readFault();
    bool bad2 = (f2 != 0) || isnan(raw2) || raw2 < -100.0f || raw2 > 1000.0f;
    if (bad2) {
      if (++max2ErrorCount >= 3) {
        max2Found = false;
        enforceWaitSensorsAndStop("MAX2 read BAD x3");
        return;
      }
    } else {
      max2ErrorCount = 0;
      lastGoodTemp2 = raw2;
    }
  }
  #endif

  if (!isnan(lastGoodTemp1)) currentTemp1 = lastGoodTemp1 + temp1_offset; else currentTemp1 = NAN;
  if (!isnan(lastGoodTemp2)) currentTemp2 = lastGoodTemp2 + temp2_offset; else currentTemp2 = NAN;

  if (!max1Found || !max2Found) {
    enforceWaitSensorsAndStop("sensor lost during read");
    return;
  }
}

void setSensorsDebug(bool on) {
  sensorsDebug = on;
}
void toggleSensorsDebug() { setSensorsDebug(!sensorsDebug); }