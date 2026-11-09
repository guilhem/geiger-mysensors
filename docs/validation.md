# Validation record

These checks cover release 1.1.0. Build and test results were obtained locally
on Linux; live Home Assistant observations describe the original installation.

| Check | Result | Scope |
|---|---|---|
| Host suite | 13 tests passed, no skips | Actual sketch with simulated peripherals, real pymysensors 0.26.0 handlers, YAML examples, packaging |
| Uno build | Passed | Arduino CLI 1.5.1, AVR 1.8.8, MySensors 2.3.2, `arduino:avr:uno` |
| Flash usage | 9,776 / 32,256 bytes | Application only; bootloader excluded |
| Static SRAM | 888 / 2,048 bytes | Compiler report; runtime stack usage not measured |
| Application HEX | Identical to the previously delivered 1.1.0 HEX | Firmware source unchanged by repository preparation |

The application HEX SHA-256 is
`b6b1ceb42da80e4c68f49333ed7a8985b1e7b00e4a238639fadfd0d6e21c0b72`.
The compiler emits unused-parameter warnings in the upstream Arduino core.

The host suite checks CPM windows, zero counts, concurrent pulse/serial activity,
retries, requests, counter and clock rollover, random extraction, repeated bytes,
and capture expiry. Controller tests parse the sketch's transcript through real
MySensors handlers. Example tests check YAML and dashboard entity references;
packaging tests check hashes, deterministic contents, and corrupt-cache rejection.
These checks do not model electrical behavior or run a Home Assistant browser.

Live Home Assistant readings with firmware 1.1.0 showed Geiger Ticks advancing
from 444 to 476, Random Samples from 13 to 14, Random Byte from 93 to 21,
and a CPM reading of 42. This establishes basic live operation, without
qualifying pulse accuracy, calibration, or entropy. See the
[hardware record](hardware-validation.md) for the physical checks still needed.

Current hosted results and release downloads are available under
[GitHub Actions](https://github.com/guilhem/geiger-mysensors/actions/workflows/check.yml)
and [Releases](https://github.com/guilhem/geiger-mysensors/releases/latest).
The workflow tests, builds, and packages each change; matching version tags
publish the checked artifacts.

The documented macOS and Home Assistant OS upload procedures have not been
verified on physical hosts. No cryptographic entropy or calibrated dose claim
is made. The archive's `hardware_tested: false` records the absence of controlled
hardware qualification; it does not negate the live observations above.
