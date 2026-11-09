# Build and check the firmware

Use this guide to change the firmware or rebuild release files. To install a
prebuilt application, use the [Home Assistant OS](install-home-assistant-os.md)
or [macOS](install-macos.md) upload guide instead.

## Requirements

From a checkout of [guilhem/geiger-mysensors](https://github.com/guilhem/geiger-mysensors),
use Python 3 with `venv` and `pip`, `make`, a C++ compiler (`g++` by default), and
[Arduino CLI 1.5.1](https://github.com/arduino/arduino-cli/releases/tag/v1.5.1)
on your `PATH`. The first dependency installation and build need network access.

The [sketch profile](../firmware/GeigerMySensors/sketch.yaml) pins Arduino AVR
**1.8.8**, MySensors **2.3.2**, and `arduino:avr:uno` (ATmega328P, 16 MHz).
[toolchain.json](../toolchain.json) records the CLI version, release version, and
source archive hashes; [requirements-test.txt](../requirements-test.txt) pins
Home Assistant's controller library, `pymysensors==0.26.0`, and PyYAML for example
checks. The commands below install both.

## Test and build

Run from the repository root:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-test.txt
make test PYTHON=.venv/bin/python
make release PYTHON=.venv/bin/python
```

`make test` requires the pinned controller dependency and runs the host suite.
`make release` repeats those tests, compiles the Uno profile, and packages:

| Local output | Contents |
|---|---|
| `dist/GeigerMySensors.hex` | Application only, without the bootloader |
| `dist/GeigerMySensors-*.zip` | Application, project sources, and unchanged Arduino AVR/MySensors source archives |
| `dist/SHA256SUMS` | SHA-256 checksum of the application HEX |
| `dist/manifest.json` | Toolchain, source dependency hashes, and packaged-file hashes |

These are generated files, not tracked repository contents. Packaging verifies
the source archive hashes and reuses downloads in `.build/source-dependencies/`.
For only an application build, run `make firmware`. None of these targets uploads
to a board or changes its bootloader, fuses, or EEPROM. Published downloads belong
on [GitHub Releases](https://github.com/guilhem/geiger-mysensors/releases/latest).

For a firmware-only host check without Python dependencies:

```sh
python3 -m unittest discover -s tests -p test_firmware.py -v
```

## Validation

Host tests execute the actual sketch with simulated peripherals: D2 falling-edge
capture, 60-second CPM windows, concurrent pulse/serial activity, retries,
requests, counters, random extraction, expiry, and clock rollover. Controller
tests parse its transcript with real pymysensors handlers; an example test checks
dashboard entity references against the customization metadata. Packaging tests
check hashes, archive contents, and corrupt-download rejection. They do not run Home
Assistant or model AVR electrical and interrupt timing. The
[check workflow](../.github/workflows/check.yml) runs `make release`; its definition
alone is not evidence of a successful hosted run or a published release.

See the [validation record](validation.md) for recorded results and the
[hardware record](hardware-validation.md#what-has-been-observed) for live
observations and remaining physical and failure-mode checks. Live values do not
qualify calibration, controlled pulse capture, entropy, or macOS upload.
