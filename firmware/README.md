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

Milestone 5 (the actual SIP call) builds on
[ESP32-SIP-Voice](https://github.com/GeorgeBregman/ESP32-SIP-Voice) (MIT) —
the guide describes the grafting points. Once that integration exists as real
code, it lives here as `openphone-fw/`.

GPIO assignments live at the top of each file; the guide's "AudioKit pin
caveat" explains why you might need to change them for your board revision.
