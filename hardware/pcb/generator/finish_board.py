#!/usr/bin/env python3
"""Post-route finisher: codec EP landings, island stitching, targeted escapes, DRC."""
import pcbnew, math
from pcbnew import FromMM as MM, VECTOR2I
def V(x,y): return VECTOR2I(MM(float(x)), MM(float(y)))

b = pcbnew.LoadBoard("/root/phonebox/model02.kicad_pcb")
gnd = b.FindNet("GND").GetNetCode()

def add_track(p, q, w=0.2):
    t = pcbnew.PCB_TRACK(b); t.SetStart(V(*p)); t.SetEnd(V(*q))
    t.SetWidth(MM(w)); t.SetLayer(pcbnew.F_Cu); t.SetNetCode(gnd); b.Add(t)
def add_via(x, y, size=0.6):
    v = pcbnew.PCB_VIA(b); v.SetPosition(V(x,y)); v.SetWidth(MM(size)); v.SetDrill(MM(size/2))
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); v.SetNetCode(gnd); b.Add(v)

# --- codec floating GND pads onto the EP sub-pads
add_track((13.5,43.55),(14.0,44.88))   # pad26 -> EP NW subpad
add_track((11.55,46.0),(12.88,46.0))   # pad4  -> EP W subpad

# --- obstacle model (foreign copper, both layers)
def build_model():
    segs, pts = [], []
    for t in b.GetTracks():
        if t.GetNetCode() == gnd: continue
        if t.GetClass() == "PCB_VIA": pts.append((t.GetPosition().x, t.GetPosition().y, t.GetWidth()/2))
        else: segs.append((t.GetStart().x, t.GetStart().y, t.GetEnd().x, t.GetEnd().y, t.GetWidth()/2))
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if p.GetNetCode() != gnd:
                pts.append((p.GetPosition().x, p.GetPosition().y, max(p.GetSize().x, p.GetSize().y)/2))
    return segs, pts
def seg_dist(px,py,x1,y1,x2,y2):
    dx,dy = x2-x1,y2-y1; L2 = dx*dx+dy*dy
    if L2==0: return math.hypot(px-x1,py-y1)
    t = max(0,min(1,((px-x1)*dx+(py-y1)*dy)/L2))
    return math.hypot(px-(x1+t*dx), py-(y1+t*dy))
def make_clr(segs, pts):
    def clr(x,y):
        d = 1e12
        for s in segs: d = min(d, seg_dist(x,y,s[0],s[1],s[2],s[3]) - s[4])
        for (px,py,r) in pts: d = min(d, math.hypot(x-px,y-py) - r)
        return d/1e6
    return clr

def fill():
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())

# --- pass 1: stitch every island whose interior has a comfortable spot
fill()
segs, pts = build_model(); clr = make_clr(segs, pts)
def islands():
    out = []
    for z in b.Zones():
        if z.GetIsRuleArea() or z.GetNetCode() != gnd or z.GetLayer() != pcbnew.F_Cu: continue
        polys = z.GetFilledPolysList(pcbnew.F_Cu)
        areas = sorted(((i, polys.COutline(i).Area()) for i in range(polys.OutlineCount())), key=lambda t: -t[1])
        for i, area in areas[1:]:
            out.append(polys.COutline(i))
    return out
hard = []
for ch in islands():
    bb = ch.BBox(); best, bestd = None, 0
    for gx in range(20):
        for gy in range(20):
            x = int(bb.GetLeft() + (gx+0.5)*bb.GetWidth()/20)
            y = int(bb.GetTop() + (gy+0.5)*bb.GetHeight()/20)
            if not ch.PointInside(VECTOR2I(x,y), int(MM(0.45))): continue
            d = clr(x, y)
            if d > bestd: best, bestd = (x/1e6, y/1e6), d
    if best and bestd >= 0.66: add_via(*best)
    elif best and bestd >= 0.46: add_via(*best, size=0.5)
    else: hard.append((ch, best, bestd))
print("hard islands:", len(hard))

# --- pass 2: escape hard islands with a path-searched track to a clean via spot
def path_ok(p1, p2, start):
    L = math.hypot(p2[0]-p1[0], p2[1]-p1[1])
    n = max(2, int(L/0.15))
    for i in range(n+1):
        x = p1[0]+(p2[0]-p1[0])*i/n; y = p1[1]+(p2[1]-p1[1])*i/n
        need = 0.27 if math.hypot(x-start[0], y-start[1]) < 1.1 else 0.31
        if clr(MM(x), MM(y)) < need: return False
    return True
for ch, best, bestd in hard:
    # start: the island's GND pad with most open surroundings
    starts = []
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if p.GetNetCode() == gnd and ch.PointInside(p.GetPosition(), 0):
                starts.append((p.GetPosition().x/1e6, p.GetPosition().y/1e6))
    if not starts: continue
    bb = ch.BBox()
    cx, cy = bb.GetCenter().x/1e6, bb.GetCenter().y/1e6
    done = False
    for start in starts:
        cands = []
        for gx in range(46):
            for gy in range(46):
                x = cx - 6.9 + gx*0.3; y = cy - 6.9 + gy*0.3
                if not (0.8 < x < 79.2 and 0.8 < y < 53.2): continue
                c = clr(MM(x), MM(y))
                if c >= 0.46:
                    cands.append((math.hypot(x-start[0], y-start[1]), x, y))
        cands.sort()
        for _, x, y in cands[:60]:
            for mid in [(x,start[1]), (start[0],y)]:
                if path_ok(start, mid, start) and path_ok(mid, (x,y), start):
                    add_track(start, mid); add_track(mid, (x,y)); add_via(x, y, 0.5)
                    print("escaped island via", round(x,2), round(y,2), "from", start)
                    done = True; break
            if done: break
        if done: break
    if not done: print("STILL STUCK:", starts[:2], "best interior", round(bestd,2))

ds = b.GetDesignSettings()
ds.m_MinThroughDrill = MM(0.2)
ds.m_CopperEdgeClearance = MM(0.25)
fill()
pcbnew.SaveBoard("/root/phonebox/model02.kicad_pcb", b)
pcbnew.WriteDRCReport(b, "/root/phonebox/model02_drc.txt", pcbnew.EDA_UNITS_MILLIMETRES, False)
print("finisher done")
