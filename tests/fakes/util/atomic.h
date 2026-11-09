#pragma once
extern bool interruptsEnabled;
struct AtomicGuard {
  bool previous = interruptsEnabled;
  bool active = true;
  AtomicGuard() { interruptsEnabled = false; }
  ~AtomicGuard() { interruptsEnabled = previous; }
  explicit operator bool() const { return active; }
  void finish() { active = false; }
};
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(mode) for (AtomicGuard guard; guard; guard.finish())
