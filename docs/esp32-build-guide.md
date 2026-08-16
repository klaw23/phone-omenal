# Building the Open Phone — ESP32 breadboard guide

This guide takes you from zero hardware experience to a rotary phone on your desk
making SIP calls through an ESP32. It's written for a software engineer: every
hardware concept is introduced the first time it's needed, every milestone ends
with something observable, and nothing asks you to trust a step you can't verify.

The shape of the project, in software terms: you're building a driver (the ESP32
firmware that speaks "telephone" out one side and SIP out the other), against a
server (Asterisk, the open-source phone switch), with hardware bring-up as a
series of integration tests that happen to involve a physical bell.

Total budget: **about $120** including tools you'll keep forever. Time: four or
five evenings, comfortably spread out.

---

## 1. Shopping list

### Electronics (~$60)

| Part | ~Price | Where | Notes |
|---|---|---|---|
| **ESP32-A1S Audio Kit** (Ai-Thinker) | $15–20 | Amazon / AliExpress | The dev board. An ESP32 with an ES8388 audio codec already wired up — this is what makes two-way call audio tractable. Any revision works; note yours (silkscreen says V2.2 usually). |
| **KS0835F SLIC module** | $8–10 | eBay / AliExpress | The magic part: generates the voltage, ring signal, and audio interface a real telephone expects. Search "KS0835F SLIC". Buy two — they're the one part with shipping lag, and a spare is cheap insurance. |
| **MT8870 DTMF decoder module** | $4 | Amazon / eBay | Decodes touch-tone beeps to 4-bit digits. **Skip this if you'll only use rotary phones** — pulse dialing needs no extra hardware. |
| **A telephone** | $5–15 | Thrift store / eBay | Any corded analog phone. Get two eventually: a touch-tone one for easy debugging, a rotary one for joy. Avoid cordless phones and anything with a wall-wart. |
| **RJ11 screw-terminal breakout** (or sacrifice a phone cord) | $5 | Amazon "RJ11 breakout" | Gives you screw terminals for the phone line so nothing needs soldering. |
| **Solderless breadboard, 830 points** | $6 | anywhere | The prototyping surface. |
| **Jumper wire kit** (M-M and M-F) | $6 | anywhere | M-F ones connect breadboard to the AudioKit's pin headers. |
| **3.5mm aux cable** (to cut in half) | $3 | anywhere | Carries audio between the AudioKit's jacks and the SLIC. |
| Resistor/capacitor kit **or** just: 2× 10k, 1× 15k resistors, 2× 4.7µF and 2× 100nF capacitors | $8 kit | Amazon "resistor capacitor assortment" | A kit is worth it; you'll use it forever. Capacitors: ceramic or film, ≥16V rating. |

### Tools (~$45, one-time)

| Tool | ~Price | Why |
|---|---|---|
| **Multimeter** | $15 | Your `printf` for electricity. Any cheap auto-ranging one (e.g. AstroAI-class) is fine. |
| **USB data cable** matching the AudioKit (usually micro-USB) | $5 | Must be a *data* cable — many charging cables have no data wires and produce maddening "no port found" symptoms. |
| **Wire stripper** | $10 | For the aux cable and phone cord. Scissors work in a pinch; strippers preserve sanity. |
| **Small screwdriver set** | $8 | Screw terminals, phone shells. |
| Optional: **alligator clip jumpers** | $6 | Clip onto phone-line wires without any termination. |

No soldering iron is required for this phase. (You'll want one eventually; that's
a later problem.)

---

## 2. Hardware crash course (15 minutes of concepts)

**GPIO.** A general-purpose pin the chip can read (input) or drive (output) as a
digital 1 (3.3V) or 0 (0V). In firmware you configure direction, then read/write.
Think of each pin as a one-bit syscall.

**3.3V vs 5V logic.** The ESP32 speaks 3.3V. A 5V signal into an ESP32 pin can
damage it over time. When a 5V device sends us a signal, we drop it through a
**voltage divider**: two resistors in series to ground, signal tapped between
them. 10k on top, 15k on the bottom turns 5V into 3V — safely readable. (Ratio =
bottom/(top+bottom) = 0.6.) Signals *from* the ESP32 *to* a 5V device are usually
fine as-is: 3.3V still counts as "high" for TTL-style inputs.

**Debouncing.** Mechanical contacts (hook switches, buttons) don't switch
cleanly — they chatter for a few milliseconds. You debounce in software:
"believe a new state only after it's been stable for 15ms." A rotary dial is
just a switch that bounces *on purpose*, ten times a second.

**Breadboard anatomy.** The two long rails down each side are power rails (mark
one red = 3.3V or 5V, one blue = GND). The middle rows connect 5 holes
horizontally per row, split by the center trench. Chips straddle the trench.

**Multimeter basics.** Set to DC volts, black probe on GND, red probe on the
thing. You'll use it for exactly three questions: "is this rail 3.3/5V?",
"is this pin high or low?", "are these two points connected?" (continuity mode,
it beeps).

**What the SLIC does.** A telephone expects the wall to feed it ~48V of loop
current, detect when the handset lifts (which closes the loop), superimpose
audio on the same two wires, and slam 60–90V of 20Hz AC onto them to ring the
bell. The KS0835F module does all of that from a 5V supply and exposes a clean
3-wire digital interface plus audio in/out:

| SLIC pin | Name | Meaning |
|---|---|---|
| 1, 2 | RING, TIP | The two phone-line wires (polarity doesn't matter for normal phones) |
| 3 | F/R | Toggle at 20Hz (while RM high) to ring the bell |
| 4 | RM | Ring mode enable |
| 5 | SHK | **Output**: high = handset lifted. Also pulses during rotary dialing |
| 6 | VR | Audio **into** the phone (earpiece) |
| 7 | VX | Audio **out of** the phone (mouthpiece) |
| 8 | NC | not connected |
| 9 | GND | ground |
| 10 | +VDC | 3.3–5V supply |
| 11 | PD | power-down control (tie low/leave managed by GPIO) |

**⚡ The one safety note.** During ringing, TIP and RING carry 60–90V AC pulses.
That's the same thing every phone line in history has carried — startling, not
dangerous, but it will get your attention. Rules: don't touch the phone-line
terminals while the ring test is running, and rewire only with USB unplugged.
Everything else on this project is 5V or less.

---

## 3. Milestone 0 — the server (no hardware needed)

Do this first, before any parts arrive. When the hardware later misbehaves,
you'll know the server side is solid — halving every future debugging search.

You need a machine for **Asterisk**, the open-source phone switch: a $5 VPS, a
Raspberry Pi, or your laptop (fine for the desk phase; the box needs to reach it
over the LAN). With the `server/` directory from this repo:

```bash
cd server
docker compose up -d
```

That starts Asterisk with three extensions defined: `101`, `102` (phones) and
`600` (an echo test — it answers and plays your own voice back).

Now install **Linphone** (free softphone) on your laptop and phone. Register two
accounts against your server: username `101` password `changeme-101` and `102`/
`changeme-102`, server = the machine's IP, transport UDP. Call `102` from `101`.
Call `600` and hear yourself.

When that works, you have a phone network. Everything after this is "build an
unusual SIP client."

> **Concept check — how SIP calls work:** registration means the client tells
> the server "extension 101 is at this IP." A call is an `INVITE` through the
> server, then audio flows as RTP packets (G.711 codec = 8kHz, 64kbps, the
> literal sound of a landline). Asterisk relays both signaling and audio, which
> is why NAT at each house doesn't break things.

---

## 4. Milestone 0.5 — toolchain and hello world

1. Install **ESP-IDF** via the official VS Code extension ("ESP-IDF" by
   Espressif → "Express install", pick the latest v5.x). This gives you the
   compiler, flasher, and serial monitor in one.
2. Plug in the AudioKit. A serial port should appear (`/dev/ttyUSB0`,
   `/dev/cu.usbserial-*`, or `COMx`). If not: it's almost always the USB cable
   (charge-only) or the CP210x driver on macOS/Windows — Espressif's docs
   "establish serial connection" page covers both.
3. New project from template → `hello_world` → set target `esp32` (the AudioKit
   is a classic ESP32, not S3) → Build, Flash, Monitor.

When `Hello world!` scrolls past at 115200 baud, your toolchain is proven.
IDF's build/flash/monitor loop is your inner dev loop from here on — treat
`idf.py flash monitor` like `npm run dev`.

---

## 5. Milestone 1 — hook detection (first wires)

**Goal:** serial log prints `OFF HOOK` / `ON HOOK` as you lift the handset.

Wiring (USB unplugged):

```
AudioKit 5V  ──► breadboard red rail ──► SLIC pin 10 (+VDC)
AudioKit GND ──► breadboard blue rail ──► SLIC pin 9 (GND)
SLIC pin 5 (SHK) ──► 10k resistor ──►(tap)──► AudioKit GPIO19
                                  (tap)──► 15k resistor ──► GND
SLIC pin 11 (PD) ──► AudioKit GPIO23
SLIC pin 1 & 2 ──► phone line (RJ11 breakout center pair, usually red+green)
```

The 10k/15k pair is the voltage divider from the crash course — SHK swings 5V
because we're feeding the SLIC 5V (rings the bell hardest). Drive PD low in
firmware (powered up).

> **AudioKit pin caveat:** this board shares pins liberally between the headers,
> SD slot, and onboard keys, and revisions differ. This guide uses GPIO 19, 21,
> 22, 23 (and 5, 13, 14, 18 later) — leave the SD slot empty and all DIP
> switches OFF. If a pin reads stuck high/low, check your revision's schematic
> (one page, findable by "ESP32 Audio Kit V2.2 schematic") and swap to a free pin;
> every pin number in the example code is a `#define` at the top.

Firmware — `firmware/examples/01_hook.c` (paste into your hello-world `main`):

```c
#define PIN_SHK  19
#define PIN_PD   23

void app_main(void) {
    gpio_config_t in = { .pin_bit_mask = 1ULL << PIN_SHK, .mode = GPIO_MODE_INPUT };
    gpio_config(&in);
    gpio_config_t out = { .pin_bit_mask = 1ULL << PIN_PD, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&out);
    gpio_set_level(PIN_PD, 0);              // SLIC awake

    bool stable = false, raw, last_raw = false;
    int64_t change_us = 0;
    while (true) {
        raw = gpio_get_level(PIN_SHK);
        if (raw != last_raw) { change_us = esp_timer_get_time(); last_raw = raw; }
        if (raw != stable && esp_timer_get_time() - change_us > 15000) {  // 15ms debounce
            stable = raw;
            printf(stable ? "OFF HOOK\n" : "ON HOOK\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
```

Lift the handset: `OFF HOOK`. That's your first hardware/software integration
test passing. (If it's inverted, your SLIC reports the opposite sense — just
flip the comparison. If it never changes, multimeter the divider tap: ~3V
off-hook, ~0V on-hook.)

## 6. Milestone 2 — make it ring

**Goal:** a physical bell rings, in proper US cadence, from your code.

Add two wires: SLIC pin 4 (RM) → GPIO22, SLIC pin 3 (F/R) → GPIO21.

```c
#define PIN_RM 22
#define PIN_FR 21

// Ring: RM high, toggle F/R at 20Hz. US cadence: 2s ringing, 4s silence.
void ring_task(void *arg) {
    while (true) {
        gpio_set_level(PIN_RM, 1);
        for (int i = 0; i < 80; i++) {                 // 80 half-cycles = 2s at 20Hz
            gpio_set_level(PIN_FR, i & 1);
            vTaskDelay(pdMS_TO_TICKS(25));
            if (off_hook()) goto answered;              // stop instantly on pickup
        }
        gpio_set_level(PIN_RM, 0);
        for (int i = 0; i < 400; i++) {                // 4s silence, still watching hook
            vTaskDelay(pdMS_TO_TICKS(10));
            if (off_hook()) goto answered;
        }
    }
answered:
    gpio_set_level(PIN_RM, 0);
    vTaskDelete(NULL);
}
```

Trigger it from a serial command or just on boot. A rotary phone's bell hammer
physically slamming because your `for` loop said so is the moment this project
hooks people. If the ring is feeble, check the 5V supply and the 100µF-worth of
capacitance near the SLIC (add a cap from the kit across 5V/GND).

## 7. Milestone 3 — dial tone

**Goal:** lift the handset, hear a real dial tone.

Audio path wiring, using the cut aux cable:
- AudioKit **headphone out** → one channel's wire → **4.7µF capacitor** → SLIC
  pin 6 (VR). The capacitor is "AC coupling" — it passes audio, blocks DC.
- SLIC pin 7 (VX) → 4.7µF capacitor → the other half of the aux cable → AudioKit
  **line/aux in**. (Which jack or pad is line-in varies by AudioKit revision —
  the schematic page from Milestone 1's caveat shows yours. Some revisions route
  only onboard mics; then solder pads or use the second jack.)
- All grounds common (cable shields to the blue rail).

For the codec you don't write a driver: clone
[ESP32-SIP-Voice](https://github.com/GeorgeBregman/ESP32-SIP-Voice) and reuse
its ES8388 init (I2C config + I2S stream at 8kHz). With the codec streaming,
dial tone is just math — North American dial tone is 350Hz + 440Hz summed:

```c
// fill I2S buffer with dial tone, 8kHz mono, 16-bit
static float ph1, ph2;
void fill_dialtone(int16_t *buf, int n) {
    for (int i = 0; i < n; i++) {
        float s = sinf(ph1) * 0.25f + sinf(ph2) * 0.25f;
        ph1 += 2*M_PI*350/8000; ph2 += 2*M_PI*440/8000;
        buf[i] = (int16_t)(s * 32767);
    }
}
```

Gate it on the hook state from Milestone 1: off-hook → stream dial tone; digit
dialed → stop. Expect to fiddle with codec output volume vs what sounds right in
the earpiece — this level-matching is the project's fussiest hour. Log
everything; adjust one variable at a time.

## 8. Milestone 4 — digits

**Rotary (no extra hardware):** a rotary dial interrupts the loop N times for
digit N, at 10 pulses/sec. So on SHK you'll see, mid-off-hook, short breaks:
~60ms low, ~40ms high. Classify: break of 20–90ms = pulse; quiet gap >300ms =
digit boundary; 10 pulses = "0".

```c
// inside the debounced hook handler:
// on->off edge while off-hook: possible pulse start (t0 = now)
// off->on edge: if 20ms < now-t0 < 90ms -> pulse_count++
// timer: 300ms with no pulses and pulse_count>0 -> digit = pulse_count%10; emit
```

Watch the serial log while you dial 4: `pulse pulse pulse pulse digit=4`.
Deeply satisfying.

**Touch-tone (MT8870 module):** power the module from **3.3V** (the CM8870 chip
in these clones runs 2.5–5.5V — at 3.3V its outputs are ESP32-safe with no
dividers; if yours proves flaky at 3.3V, run it at 5V and divide all five
outputs like SHK). Audio input of the module connects across VX/GND through its
onboard network. Wire `StD` (data valid) → GPIO18, `Q1..Q4` → GPIO5, 13, 14, 15.
On StD's rising edge, read Q1–Q4 as a nibble: 1–9 are literal, 10 = `0`,
11 = `*`, 12 = `#`.

```c
static void IRAM_ATTR dtmf_isr(void *arg) {
    int d = gpio_get_level(PIN_Q1) | gpio_get_level(PIN_Q2)<<1
          | gpio_get_level(PIN_Q3)<<2 | gpio_get_level(PIN_Q4)<<3;
    // queue it; 10 means '0'
}
```

## 9. Milestone 5 — the phone call

**Goal:** dial `600` on the rotary phone, hear yourself echo back from Asterisk.
Then dial `102` and your laptop rings.

ESP32-SIP-Voice already implements registration, INVITE/BYE, and G.711 RTP with
ES8388 audio — on its own it's a working softphone with buttons. Your work is
replacing its button UI with the telephone state machine you've now built piece
by piece:

```
        IDLE ──off-hook──► DIALTONE ──first digit──► COLLECTING
          ▲                                             │ 3 digits (or 4s timeout)
          │ on-hook (anywhere)                          ▼
        [end call, stop tones]                       CALLING ──SIP 200──► IN_CALL
                                                        │                    ▲
        RINGING ◄──incoming INVITE── IDLE               └──486 busy──► BUSY_TONE
           │ off-hook ──────────────────────────────────────────────────► IN_CALL
```

Concretely: configure the repo's WiFi + SIP settings (server IP, user `101`,
password) so it registers — verify with `docker exec -it <asterisk> asterisk
-rx "pjsip show contacts"`. Then graft: collected digits → its call-start
function; its incoming-call callback → your `ring_task`; hook events → its
answer/hangup functions; in-call, its audio pipeline already moves codec ↔ RTP,
which is now phone ↔ RTP because of your Milestone-3 wiring. Busy tone
(480+620Hz, 0.5s on/off) and ring-back are your dial-tone generator with
different constants — landline nostalgia is mostly a table of sine frequencies.

There will be an evening of "registers but no audio" or "audio one way" —
that's telephony's hazing ritual. It's almost always: wrong RTP address
(Asterisk `direct_media` must stay off), codec mismatch (force G.711/`ulaw` both
ends), or a muted codec channel. `asterisk -rx "rtp set debug on"` shows you
packets in real time, and Wireshark speaks SIP natively if you want the whole
conversation.

## 10. Milestone 6 — box it

Print `hardware/enclosure/model01_*.stl` (the design in this repo sized for
exactly these modules: AudioKit bay, 60×40 perfboard posts for SLIC + MT8870
when you graduate from breadboard, keystone RJ11 opening). PETG or PLA, lid
face-down, supports only under the rear slot. Assembly: four M3×12 into the
posts, RJ11 keystone snaps into the front. The rotary-dial vent pattern on the
lid is load-bearing for morale.

When you outgrow the breadboard entirely, `hardware/pcb/` contains the next
chapter: a custom 80×54mm board (KiCad file, Gerbers, BOM) that collapses all
of this into one $25 assembled PCB — same SLIC module, same firmware, same
enclosure design language.

---

## 11. Troubleshooting

| Symptom | Usual culprit |
|---|---|
| No serial port | Charge-only USB cable (swap it), then CP210x driver |
| Boot loops / flash fails | Hold BOOT during flash; check DIP switches off; try lower baud |
| SHK never changes | Divider mis-wired; multimeter the tap: ~3V off-hook. Or PD pin left high |
| Rings weakly / not at all | 5V rail sagging — add bulk capacitance; check RM/FR wires; some phones' ringers have a switch (ringer OFF slider!) |
| Ring never stops on pickup | Your ring loop isn't checking the hook — see Milestone 2's `goto answered` |
| Dial tone distorted/quiet | Codec volume vs SLIC level; adjust ES8388 output gain first, resistor divider second |
| Hum/buzz in audio | Grounds not common; audio wires running parallel to ring wires — separate them |
| DTMF misses digits | Module starved of signal — check its input cap wiring to VX; try 5V supply + dividers |
| Registers, no audio | `disallow=all` + `allow=ulaw` both ends; `direct_media=no`; RTP debug on |
| Audio one way | NAT/direct media again, or one coupling cap backwards/missing |
| WiFi drops mid-call | Power save: `esp_wifi_set_ps(WIFI_PS_NONE);` |

## 12. References

- RetroPhone (ESP32 + this exact SLIC, brilliant wiring notes): https://github.com/ktownsend-personal/RetroPhone
- ESP32-SIP-Voice (the SIP stack you'll build on, MIT): https://github.com/GeorgeBregman/ESP32-SIP-Voice
- KS0835F breakout + docs: https://github.com/GadgetReboot/KS0835F_Phone_SLIC
- ESP-IDF docs: https://docs.espressif.com/projects/esp-idf/
- Asterisk PJSIP config: https://docs.asterisk.org/
- Hobbyist phone networks for inspiration/federation: https://phreaknet.org/ · https://npstn.us
