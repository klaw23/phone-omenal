#!/usr/bin/env python3
"""Parse Specctra .ses and write routed tracks/vias into the KiCad board."""
import re, pcbnew
from pcbnew import VECTOR2I

def tokenize(s):
    s = s.replace("(", " ( ").replace(")", " ) ")
    toks, i, out = s.split(), 0, None
    def parse(idx):
        node = []
        while idx < len(toks):
            t = toks[idx]
            if t == "(":
                child, idx = parse(idx + 1)
                node.append(child)
            elif t == ")":
                return node, idx + 1
            else:
                node.append(t.strip('"'))
                idx += 1
        return node, idx
    tree, _ = parse(0)
    return tree

def find_all(node, tag):
    out = []
    if isinstance(node, list):
        if node and node[0] == tag:
            out.append(node)
        for ch in node:
            out.extend(find_all(ch, tag))
    return out

U = 10000.0  # ses units per mm (resolution um 10)
def to_kicad(x, y):  # ses y is negated kicad y
    return VECTOR2I(int(float(x) / U * 1e6), int(-float(y) / U * 1e6))

ses = open("/root/phonebox/model02.ses").read()
tree = tokenize(ses)
b = pcbnew.LoadBoard("/root/phonebox/model02_placed.kicad_pcb")
LAYERS = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}

nt, nv, skipped = 0, 0, []
for net in find_all(tree, "net"):
    name = net[1]
    ni = b.FindNet(name)
    if ni is None:
        skipped.append(name); continue
    code = ni.GetNetCode()
    for wire in find_all(net, "wire"):
        for path in find_all(wire, "path"):
            layer, width, coords = path[1], float(path[2]), path[3:]
            pts = [to_kicad(coords[i], coords[i+1]) for i in range(0, len(coords), 2)]
            for a, c in zip(pts, pts[1:]):
                t = pcbnew.PCB_TRACK(b)
                t.SetStart(a); t.SetEnd(c)
                t.SetWidth(int(width / U * 1e6))
                t.SetLayer(LAYERS[layer])
                t.SetNetCode(code)
                b.Add(t); nt += 1
    for via in find_all(net, "via"):
        pad, x, y = via[1], via[2], via[3]
        m = re.search(r"_(\d+):(\d+)_um", pad)
        size, drill = (int(m.group(1)), int(m.group(2))) if m else (700, 350)
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(to_kicad(x, y))
        v.SetWidth(int(size * 1000)); v.SetDrill(int(drill * 1000))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(code)
        b.Add(v); nv += 1

print(f"tracks: {nt}  vias: {nv}  skipped nets: {skipped}")
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
pcbnew.SaveBoard("/root/phonebox/model02.kicad_pcb", b)
pcbnew.WriteDRCReport(b, "/root/phonebox/model02_drc.txt", pcbnew.EDA_UNITS_MILLIMETRES, False)
print("saved model02.kicad_pcb + DRC report")
