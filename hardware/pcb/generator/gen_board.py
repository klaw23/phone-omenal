#!/usr/bin/env python3
"""Generate model02.kicad_pcb: outline, footprints placed, full netlist, GND pours.
Frame: origin = board top-left, +y down. y=0 rear edge (USB/antenna), y=54 front (RJ11)."""
import pcbnew
from pcbnew import FromMM as MM, VECTOR2I

FP = "/usr/share/kicad/footprints/"
board = pcbnew.CreateEmptyBoard()

def V(x, y): return VECTOR2I(MM(float(x)), MM(float(y)))

# ---------------- parts: ref -> (lib, footprint, x, y, rot, value)
PARTS = {
 "U1": ("RF_Module", "ESP32-S3-WROOM-1",                16.0, 12.4,   0, "ESP32-S3-WROOM-1-N8R2"),
 "U2": ("Package_DFN_QFN", "QFN-28-1EP_5x5mm_P0.5mm_EP3.35x3.35mm", 14.0, 46.0, 0, "ES8388"),
 "U3": ("Package_SO", "SOIC-18W_7.5x11.6mm_P1.27mm",    60.5, 44.5,  90, "HT9170D"),
 "U4": ("Package_TO_SOT_SMD", "SOT-23-5",               58.5, 17.0,   0, "SY8089AAAC"),
 "U5": ("Package_TO_SOT_SMD", "SOT-23-6",               47.0, 15.5,   0, "USBLC6-2SC6"),
 "J1": ("Connector_RJ", "RJ14_Connfly_DS1133-S4_Horizontal", 25.0, 47.0, 270, "RJ11-6P4C"),
 "J2": ("Connector_USB", "USB_C_Receptacle_HRO_TYPE-C-31-M-12", 55.0, 5.8, 0, "USB-C 16pin"),
 "J3": ("Connector_PinSocket_2.54mm", "PinSocket_1x11_P2.54mm_Horizontal", 29.3, 24.0, 90, "KS0835F socket"),
 "F1": ("Fuse", "Fuse_1812_4532Metric",                 55.0, 12.5,   0, "1A polyfuse"),
 "D1": ("Diode_SMD", "D_SMB",                           38.0, 50.5,   0, "SMBJ100CA"),
 "L1": ("Inductor_SMD", "L_1210_3225Metric",            63.5, 16.5,   0, "2.2uH"),
 "X1": ("Crystal", "Crystal_SMD_HC49-SD",               73.0, 33.0,  90, "3.579545MHz"),
 "Y1": ("LED_SMD", "LED_WS2812B-2020_PLCC4_2.0x2.0mm",  60.0, 52.0, 180, "WS2812B-2020"),
 "SW1":("Button_Switch_SMD", "SW_SPST_TL3342",           6.5, 46.0,   0, "EN"),
 "SW2":("Button_Switch_SMD", "SW_SPST_TL3342",          70.0,  3.7,   0, "BOOT"),
 # resistors 0603
 "R1": ("Resistor_SMD", "R_0603_1608Metric", 47.0, 19.8,  0, "5.1k"),
 "R2": ("Resistor_SMD", "R_0603_1608Metric", 62.0, 13.0,  0, "5.1k"),
 "R4": ("Resistor_SMD", "R_0603_1608Metric", 21.8, 39.3,  0, "100R"),
 "R6": ("Resistor_SMD", "R_0603_1608Metric", 32.0, 37.0,  0, "10k"),
 "R7": ("Resistor_SMD", "R_0603_1608Metric", 40.0, 37.0,  0, "100k"),
 "R8": ("Resistor_SMD", "R_0603_1608Metric", 44.0, 37.0,  0, "100k"),
 "R10":("Resistor_SMD", "R_0603_1608Metric", 50.0, 20.0,  0, "10k"),
 "R11":("Resistor_SMD", "R_0603_1608Metric", 53.5, 20.0,  0, "15k"),
 "R12":("Resistor_SMD", "R_0603_1608Metric", 48.0, 37.0,  0, "300k"),
 "R13":("Resistor_SMD", "R_0603_1608Metric", 52.0, 51.0,  0, "330R"),
 "R14":("Resistor_SMD", "R_0603_1608Metric",  6.0, 28.0, 90, "10k"),
 "R15":("Resistor_SMD", "R_0603_1608Metric", 54.0, 18.0,  0, "100k"),
 "R16":("Resistor_SMD", "R_0603_1608Metric", 57.5, 19.5,  0, "22k"),
 "RP1":("Resistor_SMD", "R_0603_1608Metric", 10.0, 30.0,  0, "4.7k"),
 "RP2":("Resistor_SMD", "R_0603_1608Metric", 10.0, 33.0,  0, "4.7k"),
 # capacitors
 "C1": ("Capacitor_SMD", "C_0805_2012Metric", 43.0, 18.0, 0, "22uF"),
 "C2": ("Capacitor_SMD", "C_0805_2012Metric", 62.5, 20.5, 0, "22uF"),
 "C3": ("Capacitor_SMD", "C_0805_2012Metric", 68.0, 20.5, 0, "22uF"),
 "C4": ("Capacitor_SMD", "C_0805_2012Metric", 28.5, 20.0, 0, "22uF"),
 "C5": ("Capacitor_SMD", "C_0805_2012Metric", 24.0, 37.0, 0, "4.7uF"),
 "C6": ("Capacitor_SMD", "C_0805_2012Metric", 28.0, 37.0, 0, "4.7uF"),
 "C7": ("Capacitor_SMD", "C_0603_1608Metric", 36.0, 37.0, 0, "100nF"),
 "C8": ("Capacitor_SMD", "C_0603_1608Metric", 52.0, 37.0,  0, "100nF"),
 "C9": ("Capacitor_SMD", "C_0603_1608Metric",  6.0, 31.5, 90, "1uF"),
 "C10":("Capacitor_SMD", "CP_Elec_6.3x5.8",   16.5, 33.5, 0, "100uF"),
 "C12":("Capacitor_SMD", "C_0603_1608Metric", 27.5, 16.5, 0, "100nF"),
 "C13":("Capacitor_SMD", "C_0603_1608Metric", 18.3, 39.5, 0, "100nF"),
 "C14":("Capacitor_SMD", "C_0603_1608Metric", 14.8, 52.0, 0, "100nF"),
 "C15":("Capacitor_SMD", "C_0603_1608Metric", 72.5, 50.0, 0, "100nF"),
 "C16":("Capacitor_SMD", "C_0603_1608Metric", 67.5, 52.0, 0, "100nF"),
 "C17":("Capacitor_SMD", "C_0805_2012Metric",  8.0, 50.5, 0, "10uF"),
 "C18":("Capacitor_SMD", "C_0805_2012Metric", 11.5, 50.5, 0, "10uF"),
 "C19":("Capacitor_SMD", "C_0603_1608Metric", 16.0, 41.5, 0, "100nF"),
 "C20":("Capacitor_SMD", "C_0603_1608Metric", 68.5, 26.5, 0, "18pF"),
 "C21":("Capacitor_SMD", "C_0603_1608Metric", 68.5, 39.0, 0, "18pF"),
 # mounting holes
 "H1": ("MountingHole", "MountingHole_2.7mm_M2.5", 33.0, 12.0, 0, ""),
 "H2": ("MountingHole", "MountingHole_2.7mm_M2.5", 70.0, 10.0, 0, ""),
 "H3": ("MountingHole", "MountingHole_2.7mm_M2.5", 10.0, 37.0, 0, ""),
 "H4": ("MountingHole", "MountingHole_2.7mm_M2.5", 70.0, 44.0, 0, ""),
}

# ---------------- netlist: net -> [(ref, padnum), ...]
NETS = {
 "GND": [("U1","1"),("U1","40"),("U1","41"),("U2","4"),("U2","18"),("U2","26"),("U2","29"),
         ("U3","6"),("U3","9"),("U4","2"),("U5","2"),("Y1","3"),
         ("R2","2"),("R1","2"),("R11","2"),("R16","2"),
         ("C1","2"),("C2","2"),("C3","2"),("C4","2"),("C8","2"),("C9","2"),("C10","2"),
         ("C12","2"),("C13","2"),("C14","2"),("C15","2"),("C16","2"),("C17","2"),("C18","2"),
         ("C19","2"),("C20","2"),("C21","2"),("J3","9"),("SW1","2"),("SW2","2"),
         ("J2","A1"),("J2","A12"),("J2","B1"),("J2","B12"),("J2","S1")],
 "VBUS_RAW": [("J2","A4"),("J2","A9"),("J2","B4"),("J2","B9"),("F1","1"),("C1","1")],
 "+5V": [("F1","2"),("U4","1"),("U4","4"),("U5","5"),("J3","10"),("Y1","1"),
         ("C2","1"),("C10","1"),("C16","1")],
 "+3V3": [("L1","2"),("U1","2"),("U2","2"),("U2","3"),("U2","16"),("U2","17"),
          ("U3","5"),("U3","10"),("U3","18"),("R14","1"),("R15","1"),("RP1","1"),("RP2","1"),
          ("C3","1"),("C4","1"),("C12","1"),("C13","1"),("C14","1"),("C15","1")],
 "BUCK_LX": [("U4","3"),("L1","1")],
 "BUCK_FB": [("U4","5"),("R15","2"),("R16","1")],
 "USB_DP": [("J2","A6"),("J2","B6"),("U5","3"),("U5","4"),("U1","14")],
 "USB_DM": [("J2","A7"),("J2","B7"),("U5","1"),("U5","6"),("U1","13")],
 "CC1": [("J2","A5"),("R1","1")],
 "CC2": [("J2","B5"),("R2","1")],
 "I2C_SDA": [("U1","39"),("U2","27"),("RP1","2")],
 "I2C_SCL": [("U1","38"),("U2","28"),("RP2","2")],
 "I2S_MCLK": [("U1","4"),("U2","1")],
 "I2S_BCLK": [("U1","5"),("U2","5")],
 "I2S_LRCK": [("U1","6"),("U2","7")],
 "I2S_DOUT": [("U1","7"),("U2","6")],    # ESP -> codec DSDIN
 "I2S_DIN":  [("U1","12"),("U2","8")],   # codec ASDOUT -> ESP
 "SLIC_SHK_5V": [("J3","5"),("R10","1")],
 "SLIC_SHK": [("R10","2"),("R11","1"),("U1","18")],
 "SLIC_RM": [("J3","4"),("U1","19")],
 "SLIC_FR": [("J3","3"),("U1","20")],
 "SLIC_PD": [("J3","11"),("U1","21")],
 "DTMF_Q1": [("U3","11"),("U1","22")],
 "DTMF_Q2": [("U3","12"),("U1","8")],
 "DTMF_Q3": [("U3","13"),("U1","10")],
 "DTMF_Q4": [("U3","14"),("U1","11")],
 "DTMF_DV": [("U3","15"),("U1","23")],
 "DTMF_EST": [("U3","16"),("R12","1")],
 "DTMF_STGT": [("U3","17"),("R12","2"),("C8","1")],
 "DTMF_VREF": [("U3","4"),("U3","1")],
 "DTMF_IN": [("U3","2"),("R7","2"),("R8","1")],
 "DTMF_GS": [("U3","3"),("R8","2")],
 "DTMF_C": [("C7","2"),("R7","1")],
 "OSC1": [("U3","7"),("X1","1"),("C20","1")],
 "OSC2": [("U3","8"),("X1","2"),("C21","1")],
 "TIP": [("J1","2"),("J3","2"),("D1","1")],
 "RING": [("J1","3"),("J3","1"),("D1","2")],
 "AUD_LOUT1": [("U2","12"),("R4","1")],
 "AUD_LOUT_C": [("R4","2"),("C5","1")],
 "VR_IN": [("C5","2"),("J3","6")],
 "VX_OUT": [("J3","7"),("C6","1"),("C7","1")],
 "VX_C": [("C6","2"),("R6","1")],
 "AUD_RIN1": [("R6","2"),("U2","23")],
 "ES_VREF": [("U2","10"),("C17","1")],
 "ES_VMID": [("U2","20"),("C18","1")],
 "ES_ADCVREF": [("U2","19"),("C19","1")],
 "EN": [("U1","3"),("R14","2"),("C9","1"),("SW1","1")],
 "BOOT": [("U1","27"),("SW2","1")],
 "LED_R": [("U1","31"),("R13","1")],
 "LED_DATA": [("R13","2"),("Y1","4")],
}

# ---------------- place footprints
fps = {}
missing = []
for ref, (lib, name, x, y, rot, val) in PARTS.items():
    fp = pcbnew.FootprintLoad(FP + lib + ".pretty", name)
    if fp is None:
        missing.append((ref, lib, name)); continue
    fp.SetPosition(V(x, y))
    fp.SetOrientationDegrees(rot)
    fp.SetReference(ref)
    fp.SetValue(val)
    board.Add(fp)
    fps[ref] = fp
if missing: print("MISSING FOOTPRINTS:", missing)

# ---------------- nets
netinfo = {}
for name in NETS:
    ni = pcbnew.NETINFO_ITEM(board, name)
    board.Add(ni)
    netinfo[name] = ni
unassigned = []
for name, pins in NETS.items():
    for ref, padnum in pins:
        if ref not in fps: unassigned.append((name, ref, padnum)); continue
        hit = False
        for pad in fps[ref].Pads():
            if pad.GetNumber() == padnum:
                pad.SetNetCode(netinfo[name].GetNetCode()); hit = True
        if not hit: unassigned.append((name, ref, padnum))
if unassigned: print("UNASSIGNED PADS:", unassigned)

# report pad numbers for connectors so we can verify mapping
for ref in ["J1","J2","X1"]:
    if ref in fps:
        print(ref, "pads:", sorted(set(p.GetNumber() for p in fps[ref].Pads())))

# ---------------- board outline (80x54, C6 chamfers)
pts = [(6,0),(74,0),(80,6),(80,48),(74,54),(6,54),(0,48),(0,6),(6,0)]
for a, b in zip(pts, pts[1:]):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(V(*a)); s.SetEnd(V(*b))
    s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1))
    board.Add(s)

# ---------------- antenna keep-out (no copper under/near antenna zone)
ko = pcbnew.ZONE(board)
ko.SetIsRuleArea(True)
ko.SetDoNotAllowCopperPour(True)
ko.SetDoNotAllowTracks(True)
ko.SetDoNotAllowVias(True)
ko.SetDoNotAllowPads(False)
ko.SetDoNotAllowFootprints(False)
ko.SetLayerSet(pcbnew.LSET.AllCuMask(2))
o = ko.Outline(); o.NewOutline()
for x, y in [(6.5,0),(25.5,0),(25.5,3.2),(6.5,3.2)]:
    o.Append(MM(x), MM(y))
board.Add(ko)

# ---------------- GND pours both layers
for layer in [pcbnew.F_Cu, pcbnew.B_Cu]:
    z = pcbnew.ZONE(board)
    z.SetLayer(layer)
    z.SetNetCode(netinfo["GND"].GetNetCode())
    o = z.Outline(); o.NewOutline()
    for x, y in [(-1,-1),(81,-1),(81,55),(-1,55)]:
        o.Append(MM(x), MM(y))
    z.SetLocalClearance(MM(0.3))
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
    z.SetMinThickness(MM(0.2))
    board.Add(z)

# ---------------- silkscreen labels
def silk(text, x, y, size=1.2, layer=pcbnew.F_SilkS):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text); t.SetPosition(V(x, y)); t.SetLayer(layer)
    t.SetTextSize(VECTOR2I(MM(size), MM(size))); t.SetTextThickness(MM(size*0.15))
    board.Add(t)
silk("OPEN PHONE CO.  MODEL 02", 40, 17.5, 1.5)
silk("TIP", 20, 44.5, 0.9); silk("RING", 30.5, 44.5, 0.9)
silk("SLIC KS0835F pin1", 29, 26.8, 0.9)
silk("open source telephone - CERN-OHL-S", 40, 30, 1.0, pcbnew.B_SilkS)

# ---------------- design rules
ds = board.GetDesignSettings()
ds.m_TrackMinWidth = MM(0.15)
ds.m_ViasMinSize = MM(0.5)
ds.m_CopperEdgeClearance = MM(0.25)
ds.m_MinThroughDrill = MM(0.2)
try:
    ds.m_NetSettings.m_DefaultNetClass.SetTrackWidth(MM(0.25))
    ds.m_NetSettings.m_DefaultNetClass.SetClearance(MM(0.2))
    ds.m_NetSettings.m_DefaultNetClass.SetViaDiameter(MM(0.7))
    ds.m_NetSettings.m_DefaultNetClass.SetViaDrill(MM(0.35))
except Exception as e:
    print("netclass defaults skipped:", e)

pcbnew.SaveBoard("/root/phonebox/model02_placed.kicad_pcb", board)
print("saved model02_placed.kicad_pcb")
print("parts:", len(fps), " nets:", len(netinfo))
