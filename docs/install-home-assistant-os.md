# Install from Home Assistant OS

Flash the prebuilt application directly from the Home Assistant host, keeping
the Uno connected by USB. This guide targets an **Uno / ATmega328P at 16 MHz
with its Arduino bootloader**, using **115200 baud**. No IDE or compiler is needed.
This upload procedure has not yet been verified on a physical Home Assistant OS host.

## Prepare the terminal and files

1. Install or open the community **Advanced SSH & Web Terminal** app under
   **Settings → Apps** (called **Add-ons** in older versions). Complete its
   SSH authentication setup as described in the
   [app documentation](https://github.com/hassio-addons/app-ssh/blob/main/ssh/DOCS.md).
   In the app's **Configuration**, add `avrdude` to `packages`, preserving any
   existing packages and other settings:

   ```yaml
   packages:
     - avrdude
   ```

   This is **app configuration**, not Home Assistant's `configuration.yaml`.
   Restart the app, check its logs, and open its Web Terminal. Its
   [configuration](https://github.com/hassio-addons/app-ssh/blob/main/ssh/config.yaml)
   exposes UART/USB devices and `/share`; leave protection mode as configured.
   Disabling protection is not a prerequisite for these steps.
2. In the app's **Web Terminal**, create the destination:

   ```sh
   mkdir -p /share/geiger
   ```

3. On your computer, download and extract the full firmware ZIP from the
   [latest release](https://github.com/guilhem/geiger-mysensors/releases/latest).
   Choose the asset containing `dist/GeigerMySensors.hex` and `dist/SHA256SUMS`,
   rather than GitHub's automatic source-code ZIP. Open your computer's terminal
   in the extracted root and copy both files with its built-in `scp`:

   ```sh
   scp -O dist/GeigerMySensors.hex dist/SHA256SUMS USER@HOST:/share/geiger/
   ```

   Replace `USER` with the app's SSH username and `HOST` with the HA host's
   address; authenticate with the password or key configured in the app.
   Port 22 is the default; for another exposed SSH port, add `-P PORT` after
   `-O`. The app defaults to SFTP disabled:
   [`-O` uses legacy SCP](https://man.openbsd.org/scp#O), so no SFTP or root-login
   changes are needed. Alternatively, use an existing Samba share or File editor
   that already accesses `/share`; File editor's default `enforce_basepath`
   blocks that path. Keep both filenames unchanged.
4. Back in the app's **Web Terminal**, verify the uploaded file:

   ```sh
   cd /share/geiger
   sha256sum -c SHA256SUMS
   ```

   Continue only if the result is `GeigerMySensors.hex: OK`.

## Release the serial port and flash

Run the following commands in the app's **Web Terminal**.

1. Under **Settings → Devices & services → MySensors**, note the **exact serial
   path already configured for the Geiger gateway**. Disable only that MySensors
   entry using its menu to release the port. Keep its configuration and entities;
   do not delete it or stop all of Home Assistant. Close any other serial monitor.
   For a first installation, identify the Uno under **Settings → System → Hardware**.
2. Set `PORT` to that exact path, replacing the example below. Never select the
   first USB serial device blindly. Confirm that the path is visible inside the app:

   ```sh
   PORT='/dev/serial/by-id/REPLACE_WITH_YOUR_GEIGER_DEVICE'
   ls -l "$PORT"
   ```

   If it is missing or access is denied, check the app's logs, device access,
   USB cable, and selected path before proceeding.
3. Upload the application:

   ```sh
   avrdude -p m328p -c arduino -P "$PORT" -b 115200 -D -U flash:w:/share/geiger/GeigerMySensors.hex:i
   ```

   Require a successful exit **and flash readback verification**. AVRDUDE verifies
   automatically; do not add `-V` to bypass it. This command writes only the
   application, leaving EEPROM, fuses, and the bootloader untouched. Flash pages
   outside the image may retain previous contents; a missing bootloader cannot
   be repaired with this procedure. See the
   [AVRDUDE options](https://avrdudes.github.io/avrdude/8.0/avrdude_4.html).
4. Re-enable the same MySensors entry. If the upload failed, restore the entry
   when you stop troubleshooting; do not interpret cached readings as a successful
   upgrade. For a first installation, follow the
   [Home Assistant setup](home-assistant.md#add-the-usb-gateway).

Resetting the Uno restarts both counters and discards unfinished random samples.
Allow **one complete minute for CPM**; the first random byte takes a variable
time, depending on incoming pulses. Cached HA states may remain visible while
waiting. Check for fresh updates with the [hardware checklist](hardware-validation.md).
For physical connections, use the [wiring instructions](../README.md#wiring).
