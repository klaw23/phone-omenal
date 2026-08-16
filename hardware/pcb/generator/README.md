# Board generator

The Model 02 PCB is *generated*, not hand-drawn: `gen_board.py` places every
footprint and builds the full netlist with KiCad's pcbnew Python API, the
freerouting autorouter routes it (via Specctra DSN/SES: `import_ses.py`), and
`finish_board.py` stitches ground islands and runs DRC. To reproduce or modify:
edit the PARTS/NETS tables in gen_board.py and re-run the pipeline — the
design doc (../model02_board_design.md) explains every connection.
