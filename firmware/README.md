# Firmware

Milestone example code referenced by the build guide
(`docs/esp32-build-guide.md`). Each file is the core of one milestone, written
for ESP-IDF v5.x on the ESP32-A1S Audio Kit. They're deliberately small —
paste into an `idf.py create-project` skeleton as you reach each milestone,
rather than cloning something monolithic you didn't write yet.

| File | Milestone |
|---|---|
| `examples/01_hook.c` | Hook detection with debounce |
| `examples/02_ring.c` | Ring cadence generator |
| `examples/03_dialtone.c` | Dial/busy/ringback tone synthesis |
| `examples/04_digits.c` | Rotary pulse counting + MT8870 DTMF |
| `examples/05_provision.c` | WiFi with captive-portal fallback + LAN config page (spec §1) |
| `examples/06_ble_config.c` | Always-on BLE config GATT service (companion to 05) |
| `ble-config.html` | Web Bluetooth page that talks to 06 — open from disk in Chrome/Edge |

Milestone 5 (the actual SIP call) builds on
[ESP32-SIP-Voice](https://github.com/GeorgeBregman/ESP32-SIP-Voice) (MIT) —
the guide describes the grafting points. Once that integration exists as real
code, it lives here as `phone-omenal-fw/`.

GPIO assignments live at the top of each file; the guide's "AudioKit pin
caveat" explains why you might need to change them for your board revision.

`05_provision.c` and `06_ble_config.c` implement
`docs/provisioning-and-switchboard-spec.md`: captive portal only when WiFi is
failing, config page on the LAN IP (`http://phone-omenal.local`) once joined,
BLE reachable always, everything PIN-gated. Unlike 01–04 they are reference
code for a real subsystem rather than a bench exercise — wire them into the
Milestone 5 project alongside the SIP stack.
