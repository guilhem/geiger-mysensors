# Install the firmware on macOS

Upload the prebuilt application to an **Uno / ATmega328P at 16 MHz with its
Arduino bootloader**, using **115200 baud**. No Arduino IDE or compiler is needed.
Download and extract the full firmware ZIP from the
[latest release](https://github.com/guilhem/geiger-mysensors/releases/latest),
then open Terminal in its root, where `dist/GeigerMySensors.hex` exists.

The original installation was flashed on a Mac. The exact procedure below has
not been verified on macOS; the later 1.1.0 live observations are
recorded in [hardware validation](hardware-validation.md).

## Verify the application

Before uploading, verify the supplied HEX file:

```sh
cd dist
shasum -a 256 -c SHA256SUMS
cd ..
```

Continue only if it reports `GeigerMySensors.hex: OK`.

## Choose AVRDUDE

Reuse an existing AVRDUDE installation or previously extracted archive first.
Set `AVRDUDE` to its executable and `AVRDUDE_CONFIG` to its matching
`avrdude.conf`, using absolute paths. If you need a tool, check the architecture:

```sh
uname -m
```

### Apple Silicon with Homebrew

On `arm64`, if Homebrew is already installed (`command -v brew`), use its native
[AVRDUDE package](https://formulae.brew.sh/formula/avrdude). Install only this tool
and its dependencies with `brew install avrdude` if needed, then select it:

```sh
AVRDUDE="$(brew --prefix)/bin/avrdude"
AVRDUDE_CONFIG="$(brew --prefix)/etc/avrdude.conf"
```

### Portable Arduino archive

On Intel, or Apple Silicon without Homebrew, use the
[official Arduino AVRDUDE 8.0 archive](https://downloads.arduino.cc/tools/avrdude_8.0-arduino.1_macOS_64bit.tar.gz).
It is **Intel-only**: Apple Silicon requires Rosetta installed separately.
The checksum below is published in Arduino's
[package index](https://downloads.arduino.cc/packages/package_index.json).

```sh
curl -fL -o avrdude-macos.tar.gz https://downloads.arduino.cc/tools/avrdude_8.0-arduino.1_macOS_64bit.tar.gz
echo '8a36cf441a3ce21622eb30b40f0b71d27864b30ea851cb795ae654ee10e2c132  avrdude-macos.tar.gz' | shasum -a 256 -c
```

Continue only if the checksum reports `OK`:

```sh
tar -xzf avrdude-macos.tar.gz
AVRDUDE="$PWD/avrdude_macOS_64bit/bin/avrdude"
AVRDUDE_CONFIG="$PWD/avrdude_macOS_64bit/etc/avrdude.conf"
```

If the extracted executable lacks execute permission, run `chmod u+x "$AVRDUDE"`. If macOS blocks it, review the warning in **System Settings → Privacy & Security** and allow this executable there; do not disable Gatekeeper globally.

## Find the board and upload

Keep the Geiger counter disconnected while uploading to the Uno. Connect the
Uno with a USB data cable. List serial ports before and after connecting it and
identify the new device:

```sh
find /dev \( -name 'cu.usbmodem*' -o -name 'cu.usbserial*' \) -print 2>/dev/null
```

Close any serial monitor using the port. Set `PORT` to the identified path, replacing the example:

```sh
PORT=/dev/cu.usbmodem1234
"$AVRDUDE" -C "$AVRDUDE_CONFIG" -p m328p -c arduino -P "$PORT" -b 115200 -D -U flash:w:dist/GeigerMySensors.hex:i
```

Require a successful exit **and flash readback verification**. AVRDUDE resets
the Uno, writes the application, and verifies it automatically; do not add `-V`.
This command does not write EEPROM, fuses, or the bootloader. Flash pages outside
the image may retain previous contents; it cannot repair a missing bootloader.
See the [AVRDUDE options](https://avrdudes.github.io/avrdude/8.0/avrdude_4.html).
If uploading fails, check the cable, port, bootloader, and competing applications.

After verification, disconnect USB before wiring the counter as described in
the [wiring instructions](../README.md#wiring). Connect the assembly to the
Home Assistant host and follow the [integration guide](home-assistant.md).
Keep an existing MySensors entry and its entities. Resetting the Uno restarts
both counters and discards unfinished samples: allow a full minute for CPM and
a variable wait for the first random byte. Check fresh updates rather than
cached states using the [hardware checklist](hardware-validation.md).
