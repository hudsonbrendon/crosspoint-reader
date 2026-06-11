#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "HalGPIO.h"

class HalClock;
extern HalClock halClock;  // Singleton

class HalClock {
  bool _available = false;
  mutable uint8_t _cachedHour = 0;
  mutable uint8_t _cachedMinute = 0;
  mutable bool _hasCachedTime = false;
  mutable unsigned long _lastPollMs = 0;

  static constexpr unsigned long CLOCK_POLL_MS = 10000;  // 10 seconds

 public:
  // Call after gpio.begin() and powerManager.begin() (I2C already initialised for X3)
  void begin();

  // True if the DS3231 RTC is present on this device
  bool isAvailable() const { return _available; }

  // Get current hour (0-23) and minute (0-59).
  // Returns false if RTC is not available.
  bool getTime(uint8_t& hour, uint8_t& minute) const;

  // Current local calendar date. Works on both hardware variants:
  //   X3: read from the DS3231 date registers.
  //   X4: read from the ESP32 system clock, valid only after an NTP sync this
  //       power cycle (survives sleep/restart, lost on power loss).
  // utcOffsetQuarterHoursBiased matches getTime()/formatTime() (48 = UTC+0).
  // Returns false (and leaves outputs untouched) when no trustworthy date exists.
  bool getDate(int16_t& year, uint8_t& month, uint8_t& day, uint16_t& dayOfYear,
               uint8_t utcOffsetQuarterHoursBiased = 48) const;

  // Convenience: true when getDate() would succeed right now.
  bool hasValidDate(uint8_t utcOffsetQuarterHoursBiased = 48) const;

  // Format time into a caller-provided buffer.
  // 24h mode produces "HH:MM" (needs >=6 bytes); 12h mode produces "H:MM AM"/"HH:MM PM" (needs >=9 bytes).
  // utcOffsetQuarterHoursBiased: biased quarter-hour offset (48 = UTC+0, 0 = UTC-12, 104 = UTC+14).
  // use12Hour: when true, format as 12-hour clock with AM/PM suffix.
  // Returns false if RTC is not available.
  bool formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased = 48, bool use12Hour = false) const;

  // Sync the DS3231 RTC from an NTP server. Requires WiFi to be connected.
  // Blocks for up to ~5s while waiting for SNTP response.
  // Returns true if the RTC was successfully updated.
  //
  // Debouncing (skip if already synced once) is enforced by the caller, not here,
  // so the HAL stays free of any app-layer settings dependency.
  bool syncFromNTP();

 private:
  bool writeTimeToRTC(uint8_t hour, uint8_t minute, uint8_t second);
};
