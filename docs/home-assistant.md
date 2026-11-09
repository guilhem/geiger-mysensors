# Connect Home Assistant

Use the built-in [MySensors integration](https://www.home-assistant.io/integrations/mysensors/).
The Uno is both the USB gateway and sensor node. For firmware upload, follow the
[Home Assistant OS](install-home-assistant-os.md) or [macOS](install-macos.md) guide.

## Add the USB gateway

1. Connect the programmed, wired Uno to the Home Assistant host.
2. Find its device under **Settings → System → Hardware**; prefer a stable
   `/dev/serial/by-id/…` path when available.
3. Open **Settings → Devices & services → Add integration → MySensors**.
   Select serial, that device path, **115200 baud**, and protocol **2.3**
   (the firmware uses MySensors 2.3.2). Leave persistence enabled.
4. Look for **Geiger Counter 0** and the entities below. Entity IDs vary:
   copy yours from the entity settings before using the examples.

## Integration contract

Node **`0`** presents four `S_CUSTOM` (`23`) children with numeric `V_VAR1` (`24`)
values over USB serial, **115200 baud, 8N1**.

| Child | Entity | First value and meaning |
|---|---|---|
| `1` | Radiation CPM | After a complete 60-second window; unit `CPM` via `V_UNIT_PREFIX` (`43`) |
| `2` | Geiger Ticks | `0` immediately, then cumulative captured pulses |
| `3` | Random Byte | `0`–`255` after eight accepted bits |
| `4` | Random Samples | `1` with the first completed byte; increments even for identical bytes |

Random entities may remain absent or unknown until the first byte completes;
a real zero byte is valid. See [expected byte timing](randomness.md#expected-rate).
Presentation and value requests resend available data without inventing startup
CPM or random samples. If discovery stalls, check the serial path and competing
serial applications, then reload the existing MySensors entry.

## Upgrade an existing installation

Keep the existing integration, persistence, and CPM entity. Node `0`, child `1`,
and its unit are unchanged; children `2`–`4` add the other readings.
Download the new application from [Releases](https://github.com/guilhem/geiger-mysensors/releases/latest)
and follow your upload guide above, including checksum and flash verification.
Disconnect power before detaching or restoring the counter wiring. Reconnect to
the Home Assistant host, wait for the first valid values, and reload the existing
entry if the additional entities are missing. Opening USB can reset the Uno;
the reset behavior below also applies to upgrades.

## History and dashboard

MySensors exposes these custom values without the statistics metadata needed
for this dashboard. Use [home-assistant-customize.yaml](../examples/home-assistant-customize.yaml):
replace all four example entity IDs with yours and merge its entries into your
existing `homeassistant: customize:` configuration. Preserve existing entries
and avoid duplicate configuration keys. It sets:

- CPM: state class `measurement` for mean/min/max statistics; the firmware
  already supplies the `CPM` unit.
- Ticks and samples: counting units and state class `total_increasing`, allowing
  counter statistics to handle resets.
- Random Byte: unit `byte`, with no state class; its value is not a measurement
  to average or accumulate.

Run **Developer tools → YAML → Check configuration**, then reload
**Core configuration** on the same page, or run `homeassistant.reload_core_config`
from **Developer tools → Actions**. Create a separate dashboard, open its raw
configuration editor, and paste
[radioactivity-dashboard.yaml](../examples/radioactivity-dashboard.yaml), replacing
its CPM entity ID in both cards. It combines a native gauge
with native five-minute mean/min/max statistics. Keep CPM in Recorder and allow
at least one statistics period after applying metadata for points to appear.
The gauge's scale is a display choice, not a safety threshold.

These cards use Home Assistant's shared graph renderer and existing more-info
click behavior: select the CPM entity to open its standard history dialog.
No HACS card, template sensor, or duplicate helper is needed.

## React to counter increases

Create an automation with a **state** trigger for your Geiger Ticks entity.
Use this template condition to ignore startup, nonnumeric states, and resets:

```jinja
{{ trigger.from_state is not none
   and trigger.to_state is not none
   and is_number(trigger.from_state.state)
   and is_number(trigger.to_state.state)
   and (trigger.to_state.state | int) > (trigger.from_state.state | int) }}
```

In the action, the received count difference is
`{{ (trigger.to_state.state | int) - (trigger.from_state.state | int) }}`.
A change from 100 to 104 represents four pulses in one update. Choose actions
and automation mode accordingly; slow actions with `mode: single` can skip
subsequent triggers. Home Assistant audio cannot guarantee one click per pulse
or precise timing. The same condition on Random Samples detects completed bytes
even when consecutive byte values match; delivery limits still apply.

## Delivery and failure behavior

- **Grouped updates:** the firmware sends observed tick changes and only the
  latest completed byte/count, without a delivery queue. Home Assistant can
  coalesce updates over about **100 ms**. Repeated transmissions or equal states
  do not prove a new pulse or byte; counter increases show progress.
- **Separate byte and sequence:** the Uno sends Random Byte before Random
  Samples, but they are separate entities. Reading the byte in a samples-triggered
  automation is not a transactional snapshot. Skipped bytes cannot be recovered.
- **Volatile counts:** both counters wrap modulo 2³² and reset on Uno restart.
  Restart also discards the partial CPM window and unfinished random capture.
  CPM needs another complete minute, and random output needs a fresh complete byte.
- **Cached values:** MySensors persistence can retain old states across USB loss
  or restart. There is no automatic stale-value timeout here; a displayed value
  does not prove a working connection. Zero CPM cannot distinguish quiet
  background from a failed detector or broken pulse wire.

For less history storage, optionally exclude Geiger Ticks, Random Byte, and
Random Samples by their actual entity IDs under the existing
[`recorder: exclude: entities:` configuration](https://www.home-assistant.io/integrations/recorder/#configure-filter).
This preserves live states and automations; retain CPM for history and statistics.
Do not create helper entities just to duplicate the source values.

The coalescing and repeated-state behavior is based on Home Assistant 2026.11.0's
[MySensors entity](https://github.com/home-assistant/core/blob/2026.11.0/homeassistant/components/mysensors/entity.py),
[update delay](https://github.com/home-assistant/core/blob/2026.11.0/homeassistant/components/mysensors/const.py),
and [sensor implementation](https://github.com/home-assistant/core/blob/2026.11.0/homeassistant/components/mysensors/sensor.py).
See [validation scope](development.md#validation) and the
[physical checklist](hardware-validation.md) for checks beyond live observation.
