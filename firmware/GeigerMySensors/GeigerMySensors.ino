// SPDX-License-Identifier: GPL-2.0-only
// Pulse capture adapted from Mark Heckler's ArduinoGeigerCounter (MIT); see NOTICE.

#define MY_GATEWAY_SERIAL
#define MY_BAUD_RATE 115200

#include <MySensors.h>
#include <util/atomic.h>

constexpr uint8_t pulsePin = 2;
constexpr uint8_t childId = 1;
constexpr uint8_t ticksChildId = 2;
constexpr uint8_t randomChildId = 3;
constexpr uint8_t samplesChildId = 4;
constexpr uint32_t windowMs = 60000;
constexpr uint32_t randomTimeoutMs = 60000;

volatile uint32_t pulseCount = 0;
volatile uint32_t totalTicks = 0;
volatile uint32_t lastPulseMs = 0;
volatile uint8_t randomPhase = 0;
volatile uint8_t randomBitCount = 0;
volatile uint8_t randomBits = 0;
uint32_t intervalStartUs = 0;
uint32_t firstIntervalUs = 0;
bool invertRandomBit = false;
volatile uint8_t latestRandomByte = 0;
volatile uint32_t randomSamples = 0;
volatile bool hasRandomByte = false;
uint32_t previousMillis = 0;
float lastCpm = 0;
bool hasMeasurement = false;
bool publishPending = false;
bool ticksPublishPending = false;
bool randomPublishPending = false;
uint32_t sentTicks = 0;
uint32_t sentRandomSamples = 0;

MyMessage cpmMessage(childId, V_VAR1);
MyMessage unitMessage(childId, V_UNIT_PREFIX);
MyMessage ticksMessage(ticksChildId, V_VAR1);
MyMessage randomMessage(randomChildId, V_VAR1);
MyMessage samplesMessage(samplesChildId, V_VAR1);

void resetRandomCapture() {
  randomPhase = 0;
  randomBitCount = 0;
  randomBits = 0;
}

void countPulse() {
  const uint32_t nowUs = micros();
  ++pulseCount;
  ++totalTicks;
  const uint32_t nowMs = millis();
  if (uint32_t(nowMs - lastPulseMs) >= randomTimeoutMs) resetRandomCapture();
  lastPulseMs = nowMs;

  // HotBits: compare A->B with C->D, never reuse an interval or boundary pulse.
  switch (randomPhase) {
    case 0:
      intervalStartUs = nowUs;
      randomPhase = 1;
      break;
    case 1:
      firstIntervalUs = nowUs - intervalStartUs;
      randomPhase = 2;
      break;
    case 2:
      intervalStartUs = nowUs;
      randomPhase = 3;
      break;
    case 3: {
      const uint32_t secondIntervalUs = nowUs - intervalStartUs;
      randomPhase = 0;
      if (firstIntervalUs == secondIntervalUs) break;
      // Alternate the comparison polarity for accepted bits, as in HotBits.
      const bool bit = (firstIntervalUs > secondIntervalUs) ^ invertRandomBit;
      invertRandomBit = !invertRandomBit;
      randomBits = uint8_t((randomBits << 1) | bit);
      if (++randomBitCount == 8) {
        latestRandomByte = randomBits;
        ++randomSamples;
        hasRandomByte = true;
        randomBitCount = 0;
        randomBits = 0;
      }
      break;
    }
  }
}

void setup() {
  pinMode(pulsePin, INPUT);
  attachInterrupt(digitalPinToInterrupt(pulsePin), countPulse, FALLING);
  previousMillis = millis();
}

void presentation() {
  sendSketchInfo("Geiger Counter", "1.1.0");
  present(childId, S_CUSTOM, "Radiation CPM");
  send(unitMessage.set("CPM"));
  present(ticksChildId, S_CUSTOM, "Geiger Ticks");
  present(randomChildId, S_CUSTOM, "Random Byte");
  present(samplesChildId, S_CUSTOM, "Random Samples");
  publishPending = true;
  ticksPublishPending = true;
  randomPublishPending = true;
}

void loop() {
  const uint32_t now = millis();
  const uint32_t elapsed = now - previousMillis;
  uint32_t count = 0;
  uint32_t ticks;
  uint32_t samples;
  uint8_t randomByte;
  bool randomAvailable;
  // Copy ISR state together; perform every serial write with interrupts enabled.
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    // Read millis inside the guard so an intervening pulse cannot appear future-dated.
    if (uint32_t(millis() - lastPulseMs) >= randomTimeoutMs) resetRandomCapture();
    ticks = totalTicks;
    samples = randomSamples;
    randomByte = latestRandomByte;
    randomAvailable = hasRandomByte;
    if (elapsed >= windowMs) {
      count = pulseCount;
      pulseCount = 0;
    }
  }
  if (elapsed >= windowMs) {
    previousMillis = now;
    lastCpm = count * (60000.0f / elapsed);
    hasMeasurement = true;
    publishPending = true;
  }
  if (publishPending && hasMeasurement) {
    publishPending = !send(cpmMessage.set(lastCpm, 2));
  }
  if (ticksPublishPending || ticks != sentTicks) {
    ticksPublishPending = !send(ticksMessage.set(ticks));
    if (!ticksPublishPending) sentTicks = ticks;
  }
  // shortcut: retain only the latest completed byte; use a queue for a lossless stream.
  if (randomAvailable && (randomPublishPending || samples != sentRandomSamples)) {
    randomPublishPending = !(send(randomMessage.set(uint32_t(randomByte)))
                             && send(samplesMessage.set(samples)));
    if (!randomPublishPending) sentRandomSamples = samples;
  }
}

void receive(const MyMessage &message) {
  if (message.getCommand() != C_REQ) return;
  if (message.getType() == V_VAR1) {
    switch (message.getSensor()) {
      case childId: publishPending = true; break;
      case ticksChildId: ticksPublishPending = true; break;
      case randomChildId:
      case samplesChildId: randomPublishPending = true; break;
    }
  } else if (message.getSensor() == childId && message.getType() == V_UNIT_PREFIX) {
    send(unitMessage.set("CPM"));
  }
}
