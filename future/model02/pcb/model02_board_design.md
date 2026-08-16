# Open Phone Co. — Model 02 board design package

A single custom PCB that replaces the ESP32 Audio Kit + perfboard + breakout modules
from the Model 01 prototype. Design goal: **compactness** — one board, one USB-C cable,
one RJ11 jack, in a box barely bigger than a credit card footprint.

This document is everything except the CAD capture itself: exact parts, exact
connections, the floorplan, and the fab strategy. Capturing it in KiCad from these
tables is mechanical work (a focused weekend); every connection is specified below.
Board outline with hole and connector positions is provided as `model02_outline.dxf`
for direct import into KiCad's Edge.Cuts layer.

---

## 1. Architecture

```
                 ┌──────────────────────────────────────────────┐
  USB-C 5V ──►──┤ protection ─► 5V rail ────────► KS0835F SLIC ─┼──► RJ11 ──► phone
                 │     │                            ▲  ▲  │      │
                 │     └─► SY8089 buck ─► 3.3V      │  │  │audio │
                 │                          │       │  │  ▼      │
  USB-C D+/D- ──┼─► ESP32-S3-WROOM-1 ◄──────┘       │  │ ES8388  │
                 │      │     │  (native USB)       │  │ codec   │
                 │      │     └── GPIO: RM,F/R,PD ──┘  │  ▲      │
                 │      │         GPIO: SHK ◄──────────┘  │      │
                 │      ├── I2S + I2C ────────────────────┘      │
                 │      └── GPIO ◄── HT9170D DTMF ◄── audio tap  │
                 └──────────────────────────────────────────────┘
```

WiFi + SIP + G.711 run on the ESP32-S3. The ES8388 codec is the board's "sound card."
The KS0835F SLIC (still the proven off-the-shelf module, mounted flat on a right-angle
header) makes the analog phone line. The HT9170D decodes touch-tones in hardware —
the RetroPhone project's hard-won lesson is that software DTMF is unreliable, and this
chip is under a dollar. Rotary pulse dialing needs no hardware at all (counted on SHK).

Why the SLIC stays a module in rev A: a from-scratch SLIC (e.g. Si3210 ProSLIC) is a
serious analog/BOM undertaking with its own transformer-less ringing circuits. The
module is $8, proven in this exact role, and hand-solders in one minute. Integrating
a real SLIC chip is the natural Model 03 once the network exists.

## 2. Bill of materials

SMT parts are JLCPCB-assembly-friendly jellybeans; check live stock at order time
(all have drop-in equivalents). THT/manual parts marked ✋.

| Ref | Part (MPN) | Package | Role | ~$ |
|---|---|---|---|---|
| U1 | ESP32-S3-WROOM-1-N8R2 | module | MCU, WiFi, native USB | 3.50 |
| U2 | ES8388 | QFN-28 | stereo audio codec | 1.50 |
| U3 | HT9170D | SOIC-18 | DTMF receiver (MT8870-compatible) | 0.60 |
| U4 | SY8089A1AAC | SOT23-5 | 5V→3.3V buck, 2A | 0.30 |
| U5 | USBLC6-2SC6 | SOT23-6 | USB ESD protection | 0.25 |
| U6 | LM358 (DNP) | SOIC-8 | audio buffer stuffing option | 0.20 |
| M1 ✋ | KS0835F SLIC module | 11-pin SIL | FXS line interface | 8.00 |
| J1 ✋ | RJ11 6P4C jack, right-angle THT | — | phone line | 0.60 |
| J2 | USB-C receptacle 16-pin (e.g. TYPE-C-31-M-12) | SMT+shell | power + programming | 0.40 |
| J3 ✋ | 11-pin right-angle female header, 2.54mm | THT | SLIC mounts flat into this | 0.30 |
| X1 | 3.579545 MHz crystal | HC-49/SMD | HT9170D colorburst clock | 0.30 |
| Y1 | WS2812B-2020 | 2020 | status LED (front light pipe) | 0.15 |
| F1 | polyfuse 1A (MF-MSMF100) | 1812 | VBUS protection | 0.10 |
| D1 | SMBJ100CA bidirectional TVS | SMB | TIP/RING surge clamp (above ring voltage!) | 0.25 |
| L1 | 2.2µH, ≥2A | 0806/1212 | buck inductor | 0.10 |
| SW1, SW2 | tactile switch | SMT | EN, BOOT | 0.20 |
| — | R/C 0603 kit per section 4 | 0603 | ~30 passives | 1.00 |

Board: 2-layer, 1.6mm FR-4, HASL, **80 × 54 mm**, corners chamfered 6mm.
Five boards SMT-assembled at JLCPCB: roughly $60–90 + the five SLIC modules.
Solder M1/J1/J3 yourself (three parts, ten minutes).

## 3. ESP32-S3 pin map

| GPIO | Net | Dir | Notes |
|---|---|---|---|
| 19, 20 | USB D−, D+ | — | native USB (fixed pins), via U5 ESD |
| 1 / 2 | I2C SDA / SCL | I/O | ES8388 control (addr 0x10), 4.7k pullups |
| 4 | I2S MCLK | out | S3 routes MCLK on any pin via GPIO matrix |
| 5 / 6 | I2S BCLK / LRCLK | out | |
| 7 | I2S DOUT → codec DIN | out | ESP32 → phone audio |
| 8 | I2S DIN ← codec DOUT | in | phone → ESP32 audio |
| 10 | SLIC SHK | in | hook status + pulse dialing, **via divider** (§4.3) |
| 11 | SLIC RM | out | ring mode enable |
| 12 | SLIC F/R | out | toggle at 20 Hz while RM high = ring |
| 13 | SLIC PD | out | SLIC power-down |
| 14, 15, 17, 18 | HT9170 Q1–Q4 | in | decoded DTMF digit |
| 21 | HT9170 DV | in | data-valid, use as interrupt |
| 38 | WS2812 DIN | out | status LED |
| 0 | BOOT switch | in | strapping pin, rear pinhole access |

Strapping pins 3, 45, 46 left unconnected/default. All chosen GPIOs avoid
flash/PSRAM-reserved pins on the WROOM-1 module.

## 4. Subsystem circuits

### 4.1 Power
- USB-C VBUS → F1 polyfuse → **5V rail** (powers SLIC directly — ring generation
  is strongest at 5V). CC1/CC2 each pulled to GND with 5.1k (advertises 5V/up-to-3A sink).
- 5V → U4 SY8089 buck (L1 1µH, 22µF in, 2×22µF out) → **3.3V rail** for ESP32-S3,
  ES8388, HT9170D. WiFi TX bursts hit ~500mA; the 2A buck loafs. (Pad-compatible
  AMS1117 fallback footprint optional but the buck runs cool and is the same price.)
- Bulk: 100µF electrolytic or 2×22µF ceramic on the 5V rail near the SLIC — ring
  bursts draw current spikes.

### 4.2 Audio path (the part that earns the board its keep)
ES8388 is stereo; the phone line is mono — use LOUT1/RIN1, leaving a spare channel.

- **To the phone:** ES8388 LOUT1 → 100Ω series → 4.7µF film/X7R coupling cap →
  SLIC **VR** (audio input). Provision R-divider pads (DNP 0Ω/open by default) to
  trim level in rev A bring-up.
- **From the phone:** SLIC **VX** → 4.7µF coupling → 10k series → ES8388 **RIN1**
  (line input, not mic input — VX is line level). PGA in the codec handles fine trim.
- **DTMF tap:** VX also feeds HT9170D via the datasheet single-ended network:
  0.1µF into IN−, 100k input R, 100k feedback R IN−→GC, IN+ to VREF. X1 crystal
  + 2×18pF across OSC1/OSC2.
- **Stuffing option:** U6 LM358 buffer footprints sit in both audio legs, bypassed
  by 0Ω links by default. If levels or impedances disappoint during bring-up
  (RetroPhone needed an op-amp here), populate U6 and its gain network instead of
  respinning. This is the cheapest insurance on the board.
- **Ring-bleed filter:** RetroPhone reports the 20Hz ring bleeding into audio.
  Provision a simple RC high-pass (DNP) at RIN1 (e.g. 300Hz corner: 10k + 47nF) —
  telephone audio is 300–3400Hz anyway, and firmware mutes the codec during ring
  cadence as the first line of defense.

### 4.3 SLIC interface (M1 on J3, lying flat)
KS0835F pinout: 1-2 RING/TIP, 3 F/R, 4 RM, 5 SHK, 6 VR, 7 VX, 8 NC, 9 GND,
10 +5V, 11 PD.

- Supply pin 10 from the 5V rail with 100µF nearby.
- **SHK is 5V logic → divider 10k/15k to GPIO10** (3.0V high). RM/F/R/PD are
  driven at 3.3V, which clears TTL V_IH at 5V supply — direct connection.
- TIP/RING → J1 RJ11 center pair, with D1 (SMBJ100CA) across the pair. The TVS
  must stand off the ~±60–90V ring pulses, hence 100V. This is a desk device with
  a 2m handset cord, not a building wiring run — heavier telecom protection
  (TISP surge protectors, line fuses) is a Model 03 concern.
- Keep-out: route TIP/RING and the SLIC's line side away from codec inputs;
  20 mil clearance minimum on the ring-voltage nets.

### 4.4 Misc
- EN: 10k pullup + 1µF to GND + SW1 to GND. BOOT: SW2 on GPIO0, reachable through
  a 2mm pinhole in the rear wall (firmware updates are OTA after first flash;
  the pinhole is the break-glass path).
- WS2812B-2020 at the front edge behind a 3mm light pipe: registration status,
  ring flash, "line claimed" colors — the box's only face.

## 5. Floorplan (80 × 54, origin front-left, dims in mm)

```
 rear  ┌──────────────────────────────────────────────┐
       │ [ESP32-S3 ▓antenna→edge]   [USB-C @ x=40]  ○ │   ○ = M2.5 holes at
       │   x=7..25, keep-out under    [BOOT pinhole]  │   (10,10)(70,10)
       │   antenna: no copper       [buck U4 + L1]    │   (10,44)(70,44)
       │                                              │
       │        [KS0835F lying flat on J3]            │   corners chamfered 6mm
       │         50 × 20 zone, x≈20..71               │   (clears the enclosure's
       │                                              │    corner screw posts)
       │ [ES8388+passives]     [HT9170D + X1]         │
       │                                              │
       │  [RJ11 J1 @ x=25]   [LED Y1 @ x=60]          │
 front └──────────────────────────────────────────────┘
```

- ESP32-S3 antenna overhangs/abuts the rear edge with full copper keep-out beneath.
- Buck loop (U4-L1-caps) tight, far corner from the codec; WS2812 data line routed
  away from audio nets.
- J1 and J2 protrude through the enclosure walls; Y1 sits 1mm behind the front wall
  light-pipe hole.
- Ground pour both layers, stitched; single reference, star only the SLIC's
  noisy 5V leg.

## 6. Fab & assembly plan

1. JLCPCB 2-layer, HASL, any color (it lives in a box). Economic SMT assembly,
   top side only — every SMT part above is from their parts-library ecosystem;
   substitute equivalents for anything out of stock at order time.
2. Hand-solder J1, J3 (and the USB-C shell tabs if you chose the THT-shell variant).
3. Push M1 (SLIC module) into J3. It lies flat over the board, max height ~13mm.
4. First power-up **without** M1: verify 5V, 3.3V, USB enumeration, flash firmware.
5. Insert M1, run bring-up in the same ladder as the breadboard: SHK detect →
   ring test → dial tone → DTMF/pulse digits → SIP call.

Order 5 boards and expect to want a rev B — that is normal and cheap (~$2/bare
board on re-order). Rev A's DNP stuffing options (LM358 path, dividers, RC filter)
exist precisely so most surprises are jumper changes, not respins.

## 7. What the compactness buys

| | Model 01 (modules) | Model 02 (custom board) |
|---|---|---|
| Boards inside | AudioKit + perfboard + 3 modules | one |
| Tallest part | SLIC standing, 20.3mm | SLIC flat, ~13mm |
| Enclosure | 145 × 101 × 39 mm | **91 × 65 × 28 mm** (≈½ the volume) |
| Cables inside | 15+ jumpers | zero |
| Cost @ qty 5 | ~$35/unit in modules | ~$25/unit assembled + module |
| Programming | USB-UART on AudioKit | native USB-C, OTA after first flash |

Enclosure files: `model02.scad`, `model02_base.stl`, `model02_lid.stl` — same
open-source enclosure language (rotary-dial vents, engraved nameplate), rebuilt
around this board's exact connector positions.

## 8. Rev A as-built (KiCad files)

The generated, routed board (`model02.kicad_pcb`) supersedes a few §2/§5 details:

- **USB-C moved to x=55** on the rear edge. The stock KiCad ESP32-S3-WROOM-1
  footprint embeds Espressif's antenna keep-out, which covers the rear-LEFT
  region — the connector now sits clear of it (better RF, too).
- **Mounting holes** ended up at (33,12), (70,10), (10,37), (70,44) after
  clearing the module, codec, and connector courtyards. The enclosure
  (`model02.scad`) and `model02_outline.dxf` are updated to match.
- ES8388 is **QFN-28** (its only package); buck uses 2.2µH with a 100k/22k
  FB divider (0.6V reference → 3.3V).
- Routing: freerouting autorouter + scripted cleanup; **DRC: 0 electrical
  violations, 0 unconnected items**. Remaining DRC entries are cosmetic
  (silkscreen label overlaps, library-sync notices).
- Honest caveat: this is a machine-routed rev A. It is electrically correct
  per the netlist and passes DRC, but no human has eyeballed the analog
  routing. Treat the first 5 boards as the bring-up batch they are.
