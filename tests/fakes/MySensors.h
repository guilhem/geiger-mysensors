#pragma once
#include <cstdint>
#include <string>

constexpr int INPUT = 0;
constexpr int FALLING = 2;
constexpr uint8_t S_CUSTOM = 23;
constexpr uint8_t V_VAR1 = 24;
constexpr uint8_t V_UNIT_PREFIX = 43;
constexpr uint8_t C_REQ = 2;

struct MyMessage {
  uint8_t child;
  uint8_t type;
  uint8_t command = 1;
  std::string payload;
  MyMessage(uint8_t sensor, uint8_t valueType) : child(sensor), type(valueType) {}
  MyMessage &set(const char *value) { payload = value; return *this; }
  MyMessage &set(float value, uint8_t precision);
  MyMessage &set(uint32_t value) { payload = std::to_string(value); return *this; }
  uint8_t getCommand() const { return command; }
  uint8_t getSensor() const { return child; }
  uint8_t getType() const { return type; }
};

uint32_t millis();
uint32_t micros();
void pinMode(uint8_t pin, int mode);
int digitalPinToInterrupt(uint8_t pin);
void attachInterrupt(int pin, void (*callback)(), int edge);
void sendSketchInfo(const char *name, const char *version);
void present(uint8_t child, uint8_t type, const char *name);
bool send(const MyMessage &message);
