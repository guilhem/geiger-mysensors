# Check your hardware

## What has been observed

The original Uno installation was flashed on a Mac. Later live Home Assistant readings
were observed with firmware **1.1.0**:

| Entity | Observed values |
|---|---|
| Geiger Ticks | 444 → 476 |
| Random Samples | 13 → 14 |
| Radiation CPM | 42 |
| Random Byte | 93 → 21 |

These observations establish basic live operation of that installation. They do
not measure pulse accuracy, electrical margins, calibration, or entropy quality.
The documented macOS and Home Assistant OS upload procedures have not been
verified on the target hardware.

## Quick installation check

1. With all power disconnected, check the board revision and connections against
   the [wiring instructions](../README.md#wiring). Keep the tube and high-voltage
   circuitry enclosed; do not adjust the high-voltage potentiometer to obtain
   a desired CPM reading.
2. After connecting USB, use the existing MySensors entry or follow the
   [Home Assistant setup](home-assistant.md#add-the-usb-gateway). Geiger Ticks can
   start at zero and should advance with detected pulses. Background varies;
   exactly 42 CPM and one HA update per pulse are not pass criteria.
3. Allow a full minute after reset for the first CPM value. Random Byte (`0`–`255`)
   and Random Samples appear only after a complete byte; their wait is variable.
   Confirm that Random Samples advances, including when consecutive bytes happen
   to match. Byte and count are separate HA states, not an atomic pair.
4. Check that values continue changing. A cached display after disconnection or
   reset is not a fresh measurement; zero CPM alone cannot prove detector health.

## Silence the buzzer

With power disconnected, remove the **J1 jumper cap** on the CAJOE board to
silence its buzzer. The pulse output continues, so the Uno can keep counting.
Replace the cap with power off to restore the sound. The ESPGeiger maintainer
identifies J1 as the beep control in
[ESPGeiger issue #16](https://github.com/steadramon/ESPGeiger/issues/16).
The mute has not been physically verified on the original installation.

## Checks still to perform

- **Electrical compatibility:** measure pulse level and width with suitable
  equipment to confirm compatibility with the 5 V Uno input. A multimeter may
  miss short pulses; keep measurements away from the high-voltage section.
- **Controlled pulse capture:** compare known input counts with ticks and CPM,
  including bursts. Use controlled low-voltage stimuli to check interval ties,
  timeout boundaries, and incomplete-byte rejection against the
  [randomness contract](randomness.md). Software simulations do not establish
  physical interrupt timing or missed-pulse limits.
- **Restart and reconnect:** restart HA and disconnect/reconnect USB; check that
  the existing entities resume without duplicates. An Uno reset restarts both
  counters, the CPM minute, and unfinished random capture. Check fresh updates
  and the [counter automation](home-assistant.md#react-to-counter-increases),
  including ignored resets and grouped tick increases.
- **Calibration and entropy:** no dose calibration, entropy characterization, or
  online random-source health tests have been performed. No dose coefficient or
  alarm threshold is supplied, and the bytes are not qualified for cryptographic use.

Record the firmware version, HEX checksum, board revision, serial path, HA
version, conditions, and actual results when performing these checks. See the
[software validation record](validation.md) for the separate simulated/build scope.
