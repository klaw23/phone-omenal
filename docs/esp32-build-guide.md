# Building the Phone-omenal — ESP32 breadboard guide

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
| **Breadboard power supply module** (MB102 or clone) | $4 | Amazon "MB102 breadboard power supply" | **Needed** — the AudioKit's headers are 3V3 only, and the SLIC wants 5V. Straddles the board and feeds the rails; runs off USB. Its ~700mA ceiling is fine until ringing (§6). |
| **Jumper wire kit** (M-M and M-F) | $6 | anywhere | M-F ones connect breadboard to the AudioKit's pin headers. |
| **3.5mm aux cable** (to cut in half) | $3 | anywhere | Carries audio between the AudioKit's jacks and the SLIC. |
| Resistor/capacitor kit **or** just: 2× 10k, 1× 15k resistors (10k + 4.7k in series substitutes for the 15k), 2× 4.7µF and 2× 100nF capacitors | $8 kit | Amazon "resistor capacitor assortment" | A kit is worth it; you'll use it forever. Capacitors: ceramic or film, ≥16V rating. |

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
over the LAN).

**Path A — Docker** (laptop, VPS, Pi 3+/64-bit): with the `server/` directory
from this repo:

```bash
cd server
docker compose up -d
```

**Path B — native install** (best for older Pis like the Pi 2, whose 32-bit ARM
makes Docker images scarce — and a Pi 2 handles this job with ease; G.711 calls
relay with near-zero CPU):

```bash
sudo apt update && sudo apt install asterisk
sudo cp server/asterisk/pjsip.conf server/asterisk/extensions.conf /etc/asterisk/
sudo systemctl restart asterisk
sudo asterisk -rx "pjsip show endpoints"    # should list 101, 102, 103
```

Shelf-duty tips for a Pi server: a proper 2A power supply (undervoltage is the
classic source of mystery flakiness), a decent SD card, and a static IP or DHCP
reservation so the phones' registration target never moves.

Either path starts Asterisk with three extensions defined: `101`, `102`, `103`
(phones) and `600` (an echo test — it answers and plays your own voice back).
Docker also starts the **switchboard web app** on `:8080` — accounts, number
assignment, and per-device call logs (see
`provisioning-and-switchboard-spec.md`). It's optional at this milestone; the
starter extensions work without it.

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

### 4a. Prerequisites the extension will *not* install for you

The ESP-IDF extension (and the newer ESP-IDF Installation Manager) downloads the
compiler, flasher, and Python environment itself — but it expects a handful of
system tools to already exist, and it refuses to go any further if one is
missing. On a fresh Mac the usual complaint is **`dfu-util`**.

Install them first:

```bash
# macOS
brew install dfu-util ccache cmake ninja
xcode-select --install          # only if `git` is missing

# Debian / Ubuntu / Raspberry Pi OS
sudo apt install git wget flex bison gperf python3 python3-venv python3-pip \
     cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0
```

On Windows, use Espressif's **offline installer** instead — it bundles all of
this and sidesteps the whole problem.

Check them before you open the editor. Every line must say a path:

```bash
for t in dfu-util ccache cmake ninja python3 git; do
  printf '%-10s %s\n' "$t" "$(command -v $t || echo MISSING)"
done
```

### 4b. macOS: if it *still* says missing after you installed it

Usually installing the tool is the whole fix — reopen VS Code and the check
passes. If a prerequisite you know is installed is still reported missing, the
cause is almost always that the extension can't see Homebrew.

Apps launched from the Dock inherit only a bare `/usr/bin:/bin:/usr/sbin:/sbin`
from `launchd`, and Homebrew lives in `/opt/homebrew/bin` (Apple Silicon) or
`/usr/local/bin` (Intel). VS Code normally works around this: on macOS it spawns
your login shell at startup and adopts its environment, so extensions do get
your real `PATH`. But that resolution can fail — an unusual shell, a `.zshrc`
that exits early under non-interactive use, or a slow profile that times out.

Check whether your login shell exposes the tool at all:

```bash
/bin/zsh -lic 'command -v dfu-util'
```

If that prints a path but the extension still disagrees, launch VS Code from a
terminal so it inherits the environment directly rather than deriving it:

1. `Cmd+Shift+P` → **Shell Command: Install 'code' command in PATH**
2. Quit VS Code with `Cmd+Q` — closing the window leaves the old process and
   its environment alive.
3. `code /path/to/phone-omenal`

Note this is per-launch, not a one-time repair: a process takes its environment
at start, so a Dock launch tomorrow gets the old behaviour again. If you need it
permanently, fix the environment rather than the launch method — make sure the
Homebrew `shellenv` line is somewhere your login shell reads unconditionally,
or set a `PATH` for GUI apps via a LaunchAgent.

### 4c. Python: beware version managers

ESP-IDF builds its own virtualenv, but it needs a real interpreter to build it
*from*. If your `python3` is a **pyenv/asdf shim**, the extension may fail to
resolve it — shims depend on shell initialisation that a GUI process never ran.

```bash
command -v python3        # a path under ~/.pyenv/shims is the warning sign
pyenv which python3       # the real binary it points at
```

If setup fails on Python, point the extension at the real binary rather than the
shim: setting `idf.pythonInstallPath`, or the custom-path field in Express
install. Prefer a **3.9–3.12** interpreter unless your IDF version's release
notes say otherwise — IDF trails new Python releases, so a bleeding-edge
Homebrew Python is a worse choice than an older managed one.

### 4d. Install ESP-IDF and flash hello world

**1. Install ESP-IDF** via the VS Code extension ("ESP-IDF" by Espressif →
"Express install"). Take **v6.0.2**, the current stable release. This gives you
the compiler, flasher, and serial monitor in one.

The installer can keep several versions side by side, and **ESP-IDF: Select
Current ESP-IDF Version** switches between them — useful if a third-party
component you pull in later has not caught up with v6 yet.

**2. Plug the AudioKit into the right socket.** The board has *two* micro-USB
ports. Use the one labelled **UART** — it's wired to the onboard CP2102
USB-to-serial bridge and carries 5V as well as data, so a single cable both
powers and flashes the board. The other port is power-only; its data lines go
nowhere, so the board lights up but no serial device ever appears. (That second
port earns its keep at Milestone 2: ringing draws far more current than a laptop
USB port likes, and you can feed it 5V/2A there while keeping the laptop on
UART.)

**3. Confirm the serial port appeared.** It's a device file, not a notification:

```bash
ls /dev/cu.*           # macOS — run before and after plugging in
ls /dev/ttyUSB*        # Linux
```

Look for a new `/dev/cu.usbserial-*` or `/dev/cu.SLAB_USBtoUART` (Windows:
a new `COMx` in Device Manager). On macOS use **`cu.*`, not `tty.*`** — the
`tty.*` node blocks waiting for a carrier-detect signal the board never
asserts, which hangs esptool in a way that looks like dead hardware.

Nothing there? This one command tells you which of two very different problems
you have:

```bash
system_profiler SPUSBDataType | grep -iE 'CP210|Silicon Labs|CH34|FTDI'
```

- **Prints something, but no `/dev/cu.*`** — the chip enumerated and the driver
  didn't bind. Uncommon on current macOS, which ships a CP210x driver; this is
  when you go get Silicon Labs' VCP driver.
- **Prints nothing** — the board isn't reaching USB at all. In order of
  likelihood: wrong socket (see step 2), a charge-only USB cable, dead board.

**4. Create the project.** In extension v2.x:

```
Cmd/Ctrl+Shift+P → ESP-IDF: New Project
```

Name it, then pick `hello_world` from **get-started** in the template chooser.
Set the target to **`esp32`** — the AudioKit is a classic ESP32, not an S3.
Then Build, Flash, Monitor.

> Older tutorials tell you to run **ESP-IDF: Show Examples Projects**. That
> command no longer exists; examples now live inside the New Project wizard.
> `ESP-IDF: Create New Empty Project` is a different thing — a bare skeleton,
> which defeats the point of a known-good smoke test. If you want a guided
> version matched to your installed extension, run `Welcome: Open Walkthrough`
> → ESP-IDF; "Creating an Example Project" is step 3.

If the wizard stops with **"no framework selected to load examples"**, the
extension does not know which ESP-IDF install to draw them from:

1. `Developer: Reload Window` — settings written during setup are often not
   live until the window reloads.
2. `ESP-IDF: Select Current ESP-IDF Version` → pick your install.
3. In the wizard's template step, check the framework dropdown actually reads
   **ESP-IDF**.

Note that extension v2.x stores this in a single setting, `idf.currentSetup`,
replacing the older `idf.espIdfPath` / `idf.toolsPath` pair that most
tutorials still reference. Setup writes it to the **workspace** it was run in,
so opening a different folder can leave you with no framework selected again —
copy it into your User settings if you want it everywhere.

When `Hello world!` scrolls past at 115200 baud, your toolchain is proven.
IDF's build/flash/monitor loop is your inner dev loop from here on — treat
`idf.py flash monitor` like `npm run dev`.

---

## 5. Milestone 1 — hook detection (first wires)

**Goal:** serial log prints `OFF HOOK` / `ON HOOK` as you lift the handset.

### 5a. Breadboard basics

Skip if you've used one. If you haven't, these three rules cover everything in
this guide.

**The long rails** down each edge run the length of the board. Marked `+` (red)
and `−` (blue). Every hole in one rail is the same electrical point, so it
doesn't matter which you use. Two things to check on an unfamiliar board: the
left and right rails are **not** joined to each other unless you jumper them,
and some boards **split each rail in the middle** — look for a gap in the
printed line. A continuity check between the two ends of a rail settles both in
seconds.

**The short rows** in the middle connect **5 holes horizontally**, and stop dead
at the centre trench. Row 12 on the left of the trench and row 12 on the right
are unrelated.

**A row is a node.** This is the idea the wiring diagrams assume. You join
components by putting their legs in the same row — there's no such thing as
attaching a wire to the middle of a resistor. When a diagram says "tap", that
means *a row you pick*, not a part you own.

**Jumper wires** are just wires with connectors, and every `→` below means "run
one between these points". M-M (pin/pin) goes hole-to-hole; M-F (pin/socket)
connects a breadboard hole to a male pin on a board. Check which your AudioKit
needs — revisions ship with pins or sockets.

Use red wire for anything reaching the red rail and black for the blue rail.
Not electrically required, but it turns a fifteen-connection debugging session
from ten minutes into ten seconds.

### 5b. Powering the SLIC — the AudioKit has no 5V pin

**The ESP32-A1S Audio Kit does not break out 5V on its pin headers.** Both power
pins are 3V3, and there are two of each because boards duplicate power pins for
convenience — all GND pins are the same net, as are all 3V3 pins. The SLIC needs
5V on +VDC, so it needs its own supply.

Don't substitute 3V3. Hook detection gets flaky and the bell won't ring properly
at Milestone 2.

The easy answer is a **breadboard power module** (the MB102 and its clones,
a few dollars) that straddles the board and feeds the rails directly:

- **Input:** USB *or* the DC barrel jack, not both. **Use USB** — it's already
  5V, and Milestone 1 draws almost nothing. The barrel jack wants 7–9V because
  its linear regulator can only step *down* and needs headroom; it exists for
  when you need more current than a USB port gives.
- **Jumpers:** one per rail, selecting `3.3V` / `OFF` / `5V` for *that rail*.
  They are not input selectors, despite sitting next to the input connectors.
  Set the rail you're building against to **5V**.
- **Pins:** typically two pairs at each end, doubled per rail row for
  mechanical stability, energising both sides' rails. Seat all of them, evenly
  and fully — a partial seat gives an intermittent connection that works until
  you nudge the board.

> **Check polarity before anything else is connected.** These modules can seat
> one row off, putting `+` on the blue rail. Power on, multimeter on DC volts,
> black on blue rail and red on red rail: **~5.0V** is right, **−5.0V** means
> reversed and would destroy the SLIC, **3.3V** means the jumper is wrong.

**The one rule that makes two supplies safe:** the AudioKit's GND and the 5V
supply's ground must be connected. Two sources with separate grounds means the
SLIC's signals have no shared reference and every reading is meaningless. Step 2
below does this.

### 5c. Wiring

Wire it all with the module's power switch **off**.

1. **AudioKit `GND` → blue rail.** Either GND pin, any hole. This is the common
   ground — it ties the AudioKit to the module's `−`, already on that rail.
2. **SLIC pin 10 (+VDC) → red rail**
3. **SLIC pin 9 (GND) → blue rail**
4. **SLIC pin 5 (SHK) → row 10**
5. **10k: one leg row 10, other leg row 15**
6. **15k: one leg row 15, other leg → blue rail**
7. **Row 15 → AudioKit `GPIO19`**
8. **SLIC pin 11 (PD) → AudioKit `GPIO23`**
9. **SLIC pins 1 & 2 → RJ11 breakout centre pair**

Steps 5–7 are the voltage divider from the crash course. SHK swings to 5V
because we feed the SLIC 5V; the divider scales that to 5V × 15/(10+15) = **3V**,
safe for the ESP32's 3.3V input. **Row 15 is the tap** — three legs share it:
the 10k, the 15k, and the jumper to the ESP32. That sharing *is* the connection.
Row numbers are arbitrary.

**No 15k?** Series resistances add, so 10k + 4.7k = 14.7k works — the tap lands
at 2.98V instead of 3V, well within the tolerance of the resistors themselves.
Wire it as 10k (row 10 → row 15), 4.7k (row 15 → row 20), 10k (row 20 → blue
rail). **Row 20 must contain only those two resistor legs** — that private
junction is what "in series" means physically. Anything else in it shorts out
part of the divider.

Drive PD low in firmware (powered up).

**On step 9:** RJ11 is the telephone connector; a breakout is a socket with
screw terminals so you needn't solder. A phone line uses only **two** wires, by
convention the middle two, coloured **red and green** — outer positions are a
second line, unused. Both are needed because a phone line is a **loop**:
current flows out one wire, through the handset, back the other. The two are
called **tip** and **ring**, after the parts of an operator's plug. Polarity
doesn't matter for a standard analog phone. Note there's no connection to your
breadboard ground here — the line floats, which is normal.

> Once powered these two wires sit at ~48V DC, rising to 60–90V AC while
> ringing. Not lethal, but a ring burst is a memorable belt. Rewire with the
> supply off, and don't hold the terminals during Milestone 2.

### 5d. Verify before you trust the firmware

Multimeter on **continuity** — the `•)))` sound-wave symbol, usually sharing a
dial position with the diode symbol `▶|`; press SELECT to toggle. Black probe in
`COM`, red in `VΩmA` (not the 10A jack). **Touch the probes together first: it
must beep.** Otherwise every test below lies to you.

Power **off**:

- **A. Red rail ↔ blue rail — must NOT beep.** A beep is a short across your
  supply. Find it before switching on.
- **B. AudioKit GND ↔ blue rail — should beep.** Confirms step 1.

Power **on**, DC volts, black probe on the blue rail throughout:

- **C. Red rail → ~5.0V** (see the polarity warning in 5b)
- **D. Row 15 → ~0V on-hook, ~3V off-hook**

**Check D is the whole milestone.** If that number moves correctly, the hardware
is right and anything remaining is firmware.

> **AudioKit pin caveat — read before wiring.** This board shares pins liberally
> between the headers, SD slot, and onboard keys, and revisions differ. On
> V2.2, **GPIO 5, 13, 18, 19 and 23 are wired to the six onboard KEY buttons**
> (19 is KEY3, 23 is KEY4), GPIO21 is amplifier control and GPIO22 drives an
> LED — which is every pin this guide reaches for. The onboard circuitry can
> fight your divider. The signature is check D reading a clean swing while the
> firmware never sees a change. If that happens, pull your revision's schematic
> (one page, findable by "ESP32 Audio Kit V2.2 schematic"), pick a header pin
> with nothing else attached, and change the `#define` — every pin number in the
> example code is one. Also note silkscreen shortens `GPIO19` to `IO19`; if
> yours prints something like `IO19 / KEY3`, the board is telling you the pin is
> already taken. Leave the SD slot empty and all DIP switches OFF regardless.

### Firmware — the paste-into-hello-world loop

Each milestone reuses the hello-world project from §4: **select all in
`main/hello_world_main.c`, replace with the milestone's code, rebuild, flash.**
One project, one file, and the diff between milestones is exactly the code you
just read. The snippets below are complete — includes and all — and match the
files in `firmware/examples/`, which are the canonical copies.

Two things about the build system before the first paste:

**How the build knows what to compile:** `main/CMakeLists.txt` lists it
explicitly —

```cmake
idf_component_register(SRCS "hello_world_main.c"
                       PRIV_REQUIRES spi_flash
                       INCLUDE_DIRS "")
```

`SRCS` names the source files; that's why replacing the *contents* of
`hello_world_main.c` needs no build changes, while adding a second `.c` file
would mean listing it here (and two files each defining `app_main` won't link).

**One edit you must make (once):** the hello-world template sets
`MINIMAL_BUILD ON` in the root `CMakeLists.txt`, so only components `main`
declares get built. The milestone code uses GPIO and the high-resolution
timer, which aren't in the default list — pasting without this edit fails with
`fatal error: driver/gpio.h: No such file or directory`. Change the
`PRIV_REQUIRES` line to:

```cmake
                       PRIV_REQUIRES spi_flash esp_driver_gpio esp_timer
```

That covers milestones 1–4. When a later paste hits the same error with a
different header, the error message itself names the component to append —
ESP-IDF prints "add X to PRIV_REQUIRES" at the bottom of the failure.

`firmware/examples/01_hook.c`:

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

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

(full file: `firmware/examples/02_ring.c` — paste it whole, this shows the shape)

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


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

(full file: `firmware/examples/03_dialtone.c` — paste it whole, this shows the shape)

```c
#include <math.h>
#include <stdint.h>
#include <stdbool.h>


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

(full file: `firmware/examples/04_digits.c` — paste it whole, this shows the shape)

```c
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"


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

(Much later, when the breadboard build is boring because it just works:
`future/model02/` holds a designed-but-unbuilt custom PCB that collapses all of
this into one small board. It's future work — ignore it for now.)

---

## 11. Troubleshooting

| Symptom | Usual culprit |
|---|---|
| No serial port | Wrong micro-USB socket (use the **UART** one, §4d), then charge-only USB cable, then CP210x driver. `system_profiler SPUSBDataType \| grep CP210` tells you which |
| Extension says a prerequisite is missing (`dfu-util`, `cmake`, `ninja`) | Install it (§4a) and reopen VS Code. If it's installed and *still* reported missing on macOS, the extension can't see Homebrew — §4b |
| "No framework selected to load examples" | Reload the window, then `ESP-IDF: Select Current ESP-IDF Version`. The setting is per-workspace (§4d) |
| ESP-IDF setup fails on Python | `python3` is probably a pyenv/asdf shim; point the extension at the real binary, Python 3.9–3.12 (§4c) |
| Boot loops / flash fails | Hold BOOT during flash; check DIP switches off; try lower baud |
| Nothing works, or readings make no sense with two supplies | AudioKit GND not tied to the 5V supply's ground — §5b |
| Module seated but a rail reads 0V or −5V | Seated a row off, or doesn't reach that rail. Measure every rail before wiring — §5b |
| `fatal error: driver/gpio.h: No such file or directory` | MINIMAL_BUILD only compiles declared components — add `esp_driver_gpio esp_timer` to `PRIV_REQUIRES` (§5's firmware intro). The error names the missing component |
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
