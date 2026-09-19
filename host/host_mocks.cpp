#include "host_mocks.h"

#include <cstdio>
#include <unordered_map>

namespace {
std::unordered_map<int, int> g_digitalPins;
std::unordered_map<int, int> g_analogPins;
std::unordered_map<int, int> g_digitalWrites;
unsigned long g_millis = 0;
}  // namespace

void mock_reset() {
  g_digitalPins.clear();
  g_analogPins.clear();
  g_digitalWrites.clear();
  g_millis = 0;
}

void mock_setDigital(int pin, int value) { g_digitalPins[pin] = value; }
void mock_setAnalog(int pin, int value) { g_analogPins[pin] = value; }
void mock_setMillis(unsigned long ms) { g_millis = ms; }
void mock_advanceMillis(unsigned long deltaMs) { g_millis += deltaMs; }
int mock_getDigitalWrite(int pin) {
  auto it = g_digitalWrites.find(pin);
  return it == g_digitalWrites.end() ? -1 : it->second;
}

void pinMode(int, int) {}

void digitalWrite(int pin, int value) { g_digitalWrites[pin] = value; }

int digitalRead(int pin) {
  auto it = g_digitalPins.find(pin);
  return it == g_digitalPins.end() ? HIGH : it->second;
}

int analogRead(int pin) {
  auto it = g_analogPins.find(pin);
  return it == g_analogPins.end() ? 0 : it->second;
}

unsigned long millis() { return g_millis; }

void MockLcd::print(float value, int decimals) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
  writeToCursor(buffer);
}

void MockLcd::writeToCursor(const std::string &text) {
  std::string &target = cursorRow_ == 0 ? row0_ : row1_;
  if (static_cast<int>(target.size()) < cursorCol_) {
    target.resize(cursorCol_, ' ');
  }
  target.replace(cursorCol_, text.size(), text);
}
