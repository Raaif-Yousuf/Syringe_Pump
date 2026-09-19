#pragma once

/**
 * @file host_mocks.h
 * @brief Minimal stand-ins for the Arduino core, AccelStepper and
 * LiquidCrystal_I2C, used only when pump_core.cpp is built for the host
 * (PUMP_HOST_BUILD). They exist so the exact same production source file
 * compiles both on the Arduino and on a development machine for testing.
 */

#include <string>

// ----- Arduino core constants -----
constexpr int HIGH = 1;
constexpr int LOW = 0;
constexpr int INPUT_PULLUP = 2;
constexpr int OUTPUT = 1;

#ifndef PI
constexpr double PI = 3.14159265358979323846;
#endif

// ----- Arduino core functions (digitalRead/analogRead/millis mocks) -----
// Tests drive these through the setter functions below; production code
// (pump_core.cpp) only ever calls the reader functions, exactly as it would
// on the real Arduino core.
void pinMode(int pin, int mode);
void digitalWrite(int pin, int value);
int digitalRead(int pin);
int analogRead(int pin);
unsigned long millis();

// Test-only controls (not part of the Arduino API) to program the mocked
// hardware inputs and inspect timing.
void mock_reset();
void mock_setDigital(int pin, int value);
void mock_setAnalog(int pin, int value);
void mock_setMillis(unsigned long ms);
void mock_advanceMillis(unsigned long deltaMs);
int mock_getDigitalWrite(int pin);

// ----- AccelStepper stand-in -----
class MockStepper {
 public:
  void setMaxSpeed(float speed) { maxSpeed_ = speed; }
  void setAcceleration(float accel) { acceleration_ = accel; }
  void setSpeed(float speed) { speed_ = speed; }
  float speed() const { return speed_; }
  void setCurrentPosition(long position) { position_ = position; }
  long currentPosition() const { return position_; }
  void runSpeed() { /* timing-accurate stepping is not needed for the pure math/state tests */ }

  float maxSpeedValue() const { return maxSpeed_; }
  float accelerationValue() const { return acceleration_; }

 private:
  float maxSpeed_ = 0.0f;
  float acceleration_ = 0.0f;
  float speed_ = 0.0f;
  long position_ = 0;
};

// ----- LiquidCrystal_I2C stand-in -----
class MockLcd {
 public:
  MockLcd() = default;
  MockLcd(int, int, int) {}

  void init() {}
  void backlight() {}
  void clear() { row0_.clear(); row1_.clear(); }
  void setCursor(int col, int row) {
    cursorCol_ = col;
    cursorRow_ = row;
  }
  void print(const char *text) { writeToCursor(text); }
  void print(float value, int decimals);

  const std::string &row(int r) const { return r == 0 ? row0_ : row1_; }

 private:
  void writeToCursor(const std::string &text);

  std::string row0_;
  std::string row1_;
  int cursorCol_ = 0;
  int cursorRow_ = 0;
};
