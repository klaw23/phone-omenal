# Phone-omenal 📞

**An open source telephone.** A box with an analog phone jack: plug in any
phone — a 1962 rotary, a Mickey Mouse phone, whatever the thrift store had —
and it calls other phones on your network through an open switchboard server
you (or anyone) can run. Everything is open: enclosure, electronics, firmware,
server.

The commercial inspiration is the Tin Can kids' landline; this is the
non-commercial, hackable, federated cousin.

## The build: ESP32 + a SLIC module + any old phone

**Start with [docs/esp32-build-guide.md](docs/esp32-build-guide.md).** It's the
whole project: complete parts and tools shopping list (~$120 all-in, no
soldering), a hardware crash course written for software engineers, and six
milestones — each one ends with something you can see or hear:

1. **Milestone 0** — stand up the Asterisk switchboard in Docker, call between
   two softphones (do this before any parts arrive)
2. **Hello world** — ESP-IDF toolchain, first flash
3. **Hook detection** — lift the handset, watch the serial log react
4. **Ring** — a real bell hammers because your for-loop said so
5. **Dial tone** — 350Hz + 440Hz through a real earpiece
6. **Digits** — decode rotary pulses (and touch-tones)
7. **The call** — dial 600 on a rotary phone, hear yourself echo back via SIP

## Repo layout

| Path | What |
|---|---|
| `docs/` | **The build guide — start here** |
| `server/` | Asterisk switchboard: `docker compose up -d` and go |
| `firmware/` | Milestone example code matching the guide |
| `hardware/enclosure/` | Model 01 printed enclosure (OpenSCAD + STLs + 3D viewer) — fits the guide's electronics once they graduate from breadboard to perfboard |
| `future/` | Future work, tucked away: the Model 02 custom PCB and its smaller enclosure. Ignore until the breadboard build is making calls |

## Licenses

- Firmware & server configs: **MIT**
- Hardware (enclosure, PCB): **CERN-OHL-S v2**
- Documentation: **CC BY-SA 4.0**

## Standing on shoulders

[RetroPhone](https://github.com/ktownsend-personal/RetroPhone) proved the
ESP32+SLIC phone interface; [ESP32-SIP-Voice](https://github.com/GeorgeBregman/ESP32-SIP-Voice)
provides the SIP stack; [Asterisk](https://www.asterisk.org/) is the switch;
[PhreakNet](https://phreaknet.org/) and [NPSTN](https://npstn.us) showed hobbyist
phone networks are a thriving genre.
