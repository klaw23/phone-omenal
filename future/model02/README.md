# Future work: Model 02 — the custom PCB

**Not the current project.** The current project is the ESP32 breadboard build
in `docs/esp32-build-guide.md`. This directory is a head start on what comes
*after* that works: collapsing the Audio Kit + perfboard + modules into a single
custom 80×54mm board, in a box half the size.

What's here, for when that day comes:

- `pcb/model02.kicad_pcb` — routed KiCad board (machine-routed rev A,
  DRC-clean electrically, **not yet reviewed by human eyes** — read
  `pcb/model02_board_design.md` §8 before spending money)
- `pcb/gerbers/` + `pcb/model02_bom.csv` + `pcb/model02_cpl_top.csv` — the
  JLCPCB upload set
- `pcb/model02_board_design.md` — the full engineering design doc
- `pcb/generator/` — the Python scripts that generate and route the board
- `model02.scad` / STLs / viewer — the matching smaller enclosure (91×65×28mm)

The right time to open this folder: the breadboard build makes calls reliably,
the firmware has settled, and you want ten of these instead of one.
