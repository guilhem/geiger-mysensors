#include <cassert>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#include "../firmware/GeigerMySensors/GeigerMySensors.ino"

uint64_t clockUs = 0;
bool interruptsEnabled = true;
bool injectPulse = false;
bool failNextSend = false;
bool pulseOnClockRead = false;
uint8_t failChild = 255;
void (*duringSend)(uint8_t) = nullptr;
void (*pulseIsr)() = nullptr;
std::vector<std::string> frames;

uint32_t millis() {
  const uint32_t now = uint32_t(clockUs / 1000);
  if (interruptsEnabled && pulseOnClockRead) {
    pulseOnClockRead = false;
    clockUs += 1000;
    interruptsEnabled = false;
    pulseIsr();
    interruptsEnabled = true;
  }
  return now;
}
uint32_t micros() { return uint32_t(clockUs); }
void pinMode(uint8_t pin, int mode) { assert(pin == 2 && mode == INPUT); }
int digitalPinToInterrupt(uint8_t pin) { assert(pin == 2); return 0; }
void attachInterrupt(int pin, void (*callback)(), int edge) {
  assert(pin == 0 && edge == FALLING);
  pulseIsr = callback;
}
MyMessage &MyMessage::set(float value, uint8_t precision) {
  std::ostringstream text;
  text << std::fixed << std::setprecision(precision) << value;
  payload = text.str();
  return *this;
}
void sendSketchInfo(const char *name, const char *version) {
  frames.push_back(std::string("0;255;3;0;11;") + name);
  frames.push_back(std::string("0;255;3;0;12;") + version);
}
void present(uint8_t child, uint8_t type, const char *name) {
  frames.push_back("0;" + std::to_string(child) + ";0;0;" + std::to_string(type) + ";" + name);
}
bool send(const MyMessage &message) {
  assert(interruptsEnabled); // USB writes must not extend the atomic section.
  if (failNextSend || message.child == failChild) {
    failNextSend = false;
    failChild = 255;
    return false;
  }
  frames.push_back("0;" + std::to_string(message.child) + ";1;0;"
                  + std::to_string(message.type) + ";" + message.payload);
  if (injectPulse && message.child == childId && message.type == V_VAR1) pulseIsr();
  if (duringSend) duringSend(message.child);
  return true;
}
void pulses(int number) {
  assert(pulseIsr && interruptsEnabled);
  for (int i = 0; i < number; ++i) pulseIsr();
}
void advance(uint32_t milliseconds) { clockUs += uint64_t(milliseconds) * 1000; loop(); }
void pulseAfter(uint32_t microseconds) {
  clockUs += microseconds;
  assert(interruptsEnabled);
  interruptsEnabled = false;
  pulseIsr();
  interruptsEnabled = true;
}
void intervalPair(uint32_t first, uint32_t second) {
  pulseAfter(1000);
  pulseAfter(first);
  pulseAfter(1000);
  pulseAfter(second);
}
void emitByte(uint8_t value) {
  for (int bit = 7; bit >= 0; --bit) {
    const bool greater = bool((value >> bit) & 1) ^ invertRandomBit;
    intervalPair(greater ? 2000 : 1000, greater ? 1000 : 2000);
  }
}
void boot(uint32_t start = 100) {
  clockUs = uint64_t(start) * 1000;
  pulseCount = 0;
  totalTicks = 0;
  lastPulseMs = 0;
  resetRandomCapture();
  intervalStartUs = 0;
  firstIntervalUs = 0;
  invertRandomBit = false;
  latestRandomByte = 0;
  randomSamples = 0;
  hasRandomByte = false;
  previousMillis = 0;
  lastCpm = 0;
  hasMeasurement = false;
  publishPending = false;
  ticksPublishPending = false;
  randomPublishPending = false;
  sentTicks = 0;
  sentRandomSamples = 0;
  injectPulse = false;
  failNextSend = false;
  pulseOnClockRead = false;
  failChild = 255;
  duringSend = nullptr;
  frames.clear();
  // The stable framework presents the gateway before invoking sketch setup().
  frames.push_back("0;255;0;0;17;2.3.2");
  presentation();
  setup();
}
void expectCpm(const char *value) {
  const std::string prefix = "0;1;1;0;24;";
  const auto sample = std::find_if(frames.rbegin(), frames.rend(), [&](const std::string &frame) {
    return frame.compare(0, prefix.size(), prefix) == 0;
  });
  assert(sample != frames.rend() && *sample == prefix + value);
}
int frameCount(const std::string &prefix) {
  return std::count_if(frames.begin(), frames.end(), [&](const std::string &frame) {
    return frame.compare(0, prefix.size(), prefix) == 0;
  });
}
void testFirstWindow() {
  boot();
  assert(frameCount("0;1;1;0;43;CPM") == 1);
  loop();
  assert(frames.back() == "0;2;1;0;24;0");
  pulses(20);
  advance(59999);
  assert(frameCount("0;1;1;0;24;") == 0);
  assert(frames.back() == "0;2;1;0;24;20");
  advance(1);
  expectCpm("20.00");
  advance(60000);
  expectCpm("0.00");
  pulses(30);
  advance(90000);
  expectCpm("20.00");
}
void testUsbOverlapAndRetry() {
  boot();
  loop();
  pulses(5);
  injectPulse = true;
  advance(60000);
  expectCpm("5.00");
  assert(pulseCount == 1);
  injectPulse = false;
  failNextSend = true;
  const auto beforeFailure = frameCount("0;1;1;0;24;");
  advance(60000);
  assert(frameCount("0;1;1;0;24;") == beforeFailure && publishPending);
  pulses(2);
  loop();
  expectCpm("1.00");
  assert(!publishPending && pulseCount == 2);
  advance(60000);
  expectCpm("2.00");
}
void testPresentationAndRequests() {
  boot();
  loop();
  MyMessage request(childId, V_VAR1);
  request.command = C_REQ;
  receive(request);
  const auto beforeMeasurement = frames.size();
  loop();
  assert(frames.size() == beforeMeasurement); // No invented startup zero.
  pulses(12);
  advance(60000);
  expectCpm("12.00");
  pulses(7);
  advance(30000);
  const auto epoch = previousMillis;
  presentation();
  loop();
  expectCpm("12.00");
  assert(previousMillis == epoch && pulseCount == 7);
  advance(30000);
  expectCpm("7.00");
  loop();
  receive(request);
  loop();
  expectCpm("7.00");
  request.type = V_UNIT_PREFIX;
  receive(request);
  assert(frames.back() == "0;1;1;0;43;CPM");
  const auto beforeIgnored = frames.size();
  request.child = 99;
  receive(request);
  request.child = childId;
  request.command = 1;
  receive(request);
  loop();
  assert(frames.size() == beforeIgnored);
}
void testClockRollover() {
  boot(UINT32_MAX - 10000);
  pulses(7);
  advance(60000);
  expectCpm("7.00");
}
void testRandomBytesTiesAndIdenticalSamples() {
  boot();
  loop();
  for (int i = 0; i < 20; ++i) intervalPair(1000, 1000);
  loop();
  assert(randomBitCount == 0 && !hasRandomByte);
  assert(frameCount("0;3;1;0;24;") == 0 && frameCount("0;4;1;0;24;") == 0);
  // Same comparison eight times alternates bit polarity to 01010101.
  for (int i = 0; i < 7; ++i) intervalPair(1000, 2000);
  loop();
  assert(!hasRandomByte && randomBitCount == 7);
  intervalPair(1000, 1000); // A tie must not consume a bit or change polarity.
  assert(randomBitCount == 7);
  intervalPair(1000, 2000);
  loop();
  assert(latestRandomByte == 85 && randomSamples == 1);
  assert(frames[frames.size()-2] == "0;3;1;0;24;85");
  assert(frames.back() == "0;4;1;0;24;1");
  emitByte(85);
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;85");
  assert(frames.back() == "0;4;1;0;24;2");
  emitByte(0);
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;0");
  emitByte(255);
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;255");
  assert(pulseCount == totalTicks && totalTicks == 212);
}
void testRandomTimeoutAndMicrosWrap() {
  boot();
  intervalPair(1000, 2000);
  pulseAfter(1000);
  assert(randomBitCount == 1 && randomPhase == 1);
  advance(60000);
  assert(randomBitCount == 0 && randomPhase == 0);
  assert(invertRandomBit);
  pulseAfter(1000);
  pulseAfter(1000);
  assert(randomPhase == 2);
  // Expiry must also work when loop() was not called during the gap.
  pulseAfter(60000000);
  assert(randomPhase == 1 && randomBitCount == 0);
  advance(60000);
  clockUs = uint64_t(UINT32_MAX) - 1500;
  emitByte(165);
  loop();
  assert(latestRandomByte == 165 && randomSamples == 1);
  // Long silence spanning a complete micros wrap cannot complete an old group.
  pulseAfter(1000);
  clockUs += uint64_t(UINT32_MAX) + 1;
  pulseAfter(1000);
  assert(randomPhase == 1 && randomBitCount == 0);
  boot(UINT32_MAX - 1000);
  intervalPair(1000, 2000);
  advance(60000);
  assert(randomBitCount == 0 && randomPhase == 0);
}
void testDisjointIntervalsAndTimeoutBoundaries() {
  boot();
  pulseAfter(1000);
  pulseAfter(1000);
  pulseAfter(5000000); // This ignored B->C gap must not enter either interval.
  pulseAfter(2000);
  assert(randomPhase == 0 && randomBitCount == 1 && randomBits == 0);
  pulseAfter(5000000); // The next group must begin here, not at the previous D.
  pulseAfter(1000);
  pulseAfter(1000);
  pulseAfter(2000);
  assert(randomPhase == 0 && randomBitCount == 2 && randomBits == 1);
  for (uint32_t gap : {59999u, 60000u, 60001u}) {
    boot();
    intervalPair(1000, 2000);
    pulseAfter(1000);
    pulseAfter(gap * 1000);
    assert(randomBitCount == (gap < 60000 ? 1 : 0));
    assert(randomPhase == (gap < 60000 ? 2 : 1));
    boot();
    intervalPair(1000, 2000);
    advance(gap);
    assert(randomBitCount == (gap < 60000 ? 1 : 0));
  }
  boot();
  intervalPair(1000, 2000);
  pulseOnClockRead = true;
  loop();
  assert(!pulseOnClockRead && randomBitCount == 1 && randomPhase == 1);
  boot();
  clockUs = uint64_t(UINT32_MAX) - 4500;
  intervalPair(1000, 3000); // The second interval crosses micros() rollover.
  assert(randomBitCount == 1 && randomBits == 0);
}
void testLiveTicksRequestsAndCounterWrap() {
  boot();
  loop();
  pulses(1);
  loop();
  assert(frames.back() == "0;2;1;0;24;1");
  pulses(3);
  loop();
  assert(frames.back() == "0;2;1;0;24;4");
  assert(pulseCount == 4);
  failChild = ticksChildId;
  pulses(1);
  loop();
  assert(ticksPublishPending && sentTicks == 4);
  loop();
  assert(frames.back() == "0;2;1;0;24;5");
  totalTicks = UINT32_MAX;
  loop();
  assert(frames.back() == "0;2;1;0;24;4294967295");
  pulses(1);
  loop();
  assert(frames.back() == "0;2;1;0;24;0");
  MyMessage request(ticksChildId, V_VAR1);
  request.command = C_REQ;
  receive(request);
  const auto beforeRequest = frames.size();
  loop();
  assert(frames.size() == beforeRequest + 1 && frames.back() == "0;2;1;0;24;0");
}
void testRandomSnapshotRetryAndReconnect() {
  boot();
  loop();
  MyMessage request(randomChildId, V_VAR1);
  request.command = C_REQ;
  receive(request);
  loop();
  assert(frameCount("0;3;1;0;24;") == 0);
  emitByte(165);
  failChild = randomChildId;
  loop();
  assert(randomPublishPending && sentRandomSamples == 0);
  assert(frameCount("0;4;1;0;24;") == 0);
  failChild = samplesChildId;
  loop();
  assert(randomPublishPending && sentRandomSamples == 0);
  loop();
  assert(!randomPublishPending && sentRandomSamples == 1);
  const auto beforeRepeat = frames.size();
  loop();
  assert(frames.size() == beforeRepeat);
  intervalPair(1000, 2000);
  const auto partialCount = pulseCount;
  presentation();
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;165");
  assert(frames.back() == "0;4;1;0;24;1");
  assert(randomBitCount == 1 && pulseCount == partialCount);
  request.child = samplesChildId;
  receive(request);
  loop();
  assert(frames.back() == "0;4;1;0;24;1");
  advance(60000); // Discard partial bits, retain the last complete byte.
  randomSamples = UINT32_MAX;
  emitByte(255);
  duringSend = [](uint8_t child) {
    if (child == randomChildId) { duringSend = nullptr; emitByte(90); }
  };
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;255");
  assert(frames.back() == "0;4;1;0;24;0");
  assert(randomSamples == 1 && latestRandomByte == 90);
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;90");
  assert(frames.back() == "0;4;1;0;24;1");
  emitByte(42);
  emitByte(123);
  loop();
  assert(frames[frames.size()-2] == "0;3;1;0;24;123");
  assert(frames.back() == "0;4;1;0;24;3");
}
int main(int argc, char **argv) {
  if (argc == 2 && std::string(argv[1]) == "--transcript") {
    boot();
    loop();
    pulses(20);
    advance(60000);
    emitByte(165);
    loop();
    emitByte(165);
    loop();
    for (const auto &frame : frames) std::cout << frame << '\n';
    return 0;
  }
  testFirstWindow();
  testUsbOverlapAndRetry();
  testPresentationAndRequests();
  testClockRollover();
  testRandomBytesTiesAndIdenticalSamples();
  testRandomTimeoutAndMicrosWrap();
  testDisjointIntervalsAndTimeoutBoundaries();
  testLiveTicksRequestsAndCounterWrap();
  testRandomSnapshotRetryAndReconnect();
  std::cout << "Firmware checks passed\n";
}
