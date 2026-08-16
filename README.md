# Open Phone Co. 📞

**An open source telephone.** A 3D-printed box with an analog phone jack: plug
in any phone — a 1962 rotary, a Mickey Mouse phone, whatever the thrift store
had — and it calls other phones on your network through an open switchboard
server you (or anyone) can run. Everything is open: enclosure, electronics,
firmware, server.

The commercial inspiration is the Tin Can kids' landline; this is the
non-commercial, hackable, federated cousin.

## Start here

**[docs/esp32-build-guide.md](docs/esp32-build-guide.md)** — the step-by-step
build: full parts + tools shopping list, a hardware crash course for software
engineers, and six milestones from `hello_world` to a rotary phone making SIP
calls. No soldering required for the breadboard phase, ~$120 all-in including
tools.

## Repo layout

| Path | What |
|---|---|
| `docs/` | The build guide |
| `server/` | Asterisk switchboard: `docker compose up -d` and go |
| `firmware/` | Milestone example code for the ESP32 (see guide) |
| `hardware/enclosure/` | Parametric OpenSCAD enclosures + printable STLs + 3D viewers. Model 01 fits the breadboard-graduate modules; Model 02 fits the custom PCB |
| `hardware/pcb/` | Model 02 custom board: routed KiCad file, Gerbers, BOM, placement files, full design doc — plus the Python scripts that *generate* the board |

## Project phases

1. **Breadboard** (the guide): ESP32 Audio Kit + KS0835F SLIC module + any phone.
2. **Model 01**: same electronics, tidied onto a perfboard inside the printed box.
3. **Model 02**: single custom 80×54mm PCB (designed, routed, DRC-clean in
   `hardware/pcb/` — machine-routed rev A, unreviewed by human eyes, see its
   design doc §8 before ordering).
4. **Switchboard as a product**: web UI over Asterisk — device claim codes,
   number assignment, parental allowlists, federation between switchboards.

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
