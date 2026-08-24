// ============================================================
//  PHONE-OMENAL — "Model 02" enclosure
//  Built around the custom 80x54mm PCB (see model02_board_design.md):
//  on-board RJ11, USB-C, WS2812 light pipe, SLIC module lying flat.
//  License: CERN-OHL-S / CC-BY-SA
//
//  part = "base" | "lid" | "components" | "display" | "exploded" | "interior"
// ============================================================

part = "display";

/* ---------- parameters (mm) ---------- */
wall = 2.4; floor_th = 2.4; lid_th = 2.8; corner_r = 5;

ix = 86; iy = 60; iz = 23;          // interior

// PCB: 80x54, chamfer C6, at (3,3); top of PCB sits at floor+4+1.6
pcb_x = 3;  pcb_y = 3;  pcb_w = 80; pcb_d = 54; pcb_ch = 6;
standoff_h = 4; pcb_top = floor_th + standoff_h + 1.6;
// mounting holes at board (10,10)(70,10)(10,44)(70,44) -> interior:
so_pts = [[36,15],[73,13],[13,40],[73,47]];  // matches PCB holes (33,12)(70,10)(10,37)(70,44)

// front-face features (board coords -> interior x = board x + 3)
rj_cx = 28;  rj_w = 14;  rj_z0 = floor_th+5.1; rj_z1 = floor_th+18.1; // RJ11 opening
led_cx = 63; led_d = 3.4; led_z = floor_th+8.6;                      // light pipe
// rear-face features
usb_cx = 58; usb_w = 10.5; usb_z0 = floor_th+5.1; usb_z1 = floor_th+10.1;
boot_cx = 73; boot_d = 2.2; boot_z = floor_th+7.6;                   // BOOT pinhole

// lid screws M2.5 into corner posts
post_r = 3.8; pin = 3.0; pilot = 2.05; shaft = 2.7; head = 5.2;
post_pts = [[pin,pin],[ix-pin,pin],[pin,iy-pin],[ix-pin,iy-pin]];

// rotary-dial vents
dial_cx = 24; dial_cy = 32; dial_r = 14; dial_hole_d = 6.5; dial_n = 10;

label_text = "PHONE-OMENAL";
label_sub  = "MODEL 02";

ox = ix + 2*wall; oy = iy + 2*wall; base_h = floor_th + iz;
$fn = 48;

/* ---------- helpers ---------- */
module rrect(w, d, r) { hull() for (x=[r,w-r], y=[r,d-r]) translate([x,y]) circle(r=r); }
module rbox(w, d, h, r) { linear_extrude(h) rrect(w, d, r); }
module chamfered_pcb() {
  polygon([[pcb_ch,0],[pcb_w-pcb_ch,0],[pcb_w,pcb_ch],[pcb_w,pcb_d-pcb_ch],
           [pcb_w-pcb_ch,pcb_d],[pcb_ch,pcb_d],[0,pcb_d-pcb_ch],[0,pcb_ch]]);
}

/* ============================================================ BASE */
module base() {
  difference() {
    union() {
      difference() {
        translate([-wall,-wall,0]) rbox(ox, oy, base_h, corner_r);
        translate([0,0,floor_th]) rbox(ix, iy, iz+1, corner_r-wall);
      }
      // corner posts (lid screws, M2.5)
      for (p=post_pts) translate([p[0],p[1],floor_th]) cylinder(r=post_r, h=iz);
      // PCB standoffs (M2.5)
      for (p=so_pts) translate([p[0],p[1],floor_th]) cylinder(r=3.0, h=standoff_h);
    }
    // post pilot holes
    for (p=post_pts) translate([p[0],p[1],floor_th+iz-10]) cylinder(d=pilot, h=10.1);
    // standoff pilot holes (into floor for thread depth)
    for (p=so_pts) translate([p[0],p[1],floor_th-1.4+standoff_h-4]) cylinder(d=pilot, h=5.5);
    // RJ11 opening (front)
    translate([rj_cx-rj_w/2, -wall-0.5, rj_z0]) cube([rj_w, wall+2, rj_z1-rj_z0]);
    // LED light-pipe hole (front)
    translate([led_cx, 0.5, led_z]) rotate([90,0,0]) cylinder(d=led_d, h=wall+1.5);
    // USB-C slot (rear), rounded ends
    hull() for (sx=[-1,1])
      translate([usb_cx+sx*(usb_w/2-(usb_z1-usb_z0)/2), iy+wall+0.5, (usb_z0+usb_z1)/2])
        rotate([90,0,0]) cylinder(d=usb_z1-usb_z0, h=wall+1.5);
    // BOOT pinhole (rear)
    translate([boot_cx, iy+wall+0.5, boot_z]) rotate([90,0,0]) cylinder(d=boot_d, h=wall+1.5);
    // engravings
    translate([rj_cx+rj_w/2+3.5, -wall+0.5, floor_th+9]) rotate([90,0,0])
      linear_extrude(1) text("LINE", size=3.2, font="Liberation Sans:style=Bold");
    translate([usb_cx-usb_w/2-3.5, iy+wall-0.5, floor_th+6]) rotate([90,0,180])
      linear_extrude(1) text("PWR", size=3.2, halign="left", font="Liberation Sans:style=Bold");
  }
}

/* ============================================================ LID */
module lid() {
  difference() {
    union() {
      translate([-wall,-wall,0]) rbox(ox, oy, lid_th, corner_r);
      // alignment tabs at edge midspans
      tab_l = 30; tab_t = 1.6; tab_h = 3.0; inset = 0.3;
      translate([ix/2-tab_l/2, inset, -tab_h])          cube([tab_l, tab_t, tab_h]);
      translate([ix/2-tab_l/2, iy-inset-tab_t, -tab_h]) cube([tab_l, tab_t, tab_h]);
      translate([inset, iy/2-tab_l/2, -tab_h])          cube([tab_t, tab_l, tab_h]);
      translate([ix-inset-tab_t, iy/2-tab_l/2, -tab_h]) cube([tab_t, tab_l, tab_h]);
    }
    // countersunk M2.5 screw holes
    for (p=post_pts) {
      translate([p[0],p[1],-0.1]) cylinder(d=shaft, h=lid_th+0.2);
      translate([p[0],p[1],lid_th-1.5]) cylinder(d1=shaft, d2=head, h=1.51);
    }
    // rotary-dial vents
    for (i=[0:dial_n-1]) {
      a = 90 + 30 + i*(300/(dial_n-1));
      translate([dial_cx+dial_r*cos(a), dial_cy+dial_r*sin(a), -0.1])
        cylinder(d=dial_hole_d, h=lid_th+0.2);
    }
    translate([dial_cx, dial_cy, -0.1]) cylinder(d=8, h=lid_th+0.2);
    // engraved dial ring
    translate([dial_cx, dial_cy, lid_th-0.6]) difference() {
      cylinder(r=dial_r+dial_hole_d/2+1.8, h=0.7);
      translate([0,0,-0.1]) cylinder(r=dial_r+dial_hole_d/2+0.8, h=0.9);
    }
    // engraved labels
    translate([80, 46, lid_th-0.6]) linear_extrude(0.7)
      text(label_text, size=4.2, halign="right", font="Liberation Sans:style=Bold");
    translate([80, 38, lid_th-0.6]) linear_extrude(0.7)
      text(label_sub, size=3.4, halign="right", font="Liberation Sans");
    translate([80, 10, lid_th-0.6]) linear_extrude(0.7)
      text("an open source telephone", size=2.8, halign="right", font="Liberation Sans:style=Italic");
  }
}

/* ============================================================ COMPONENTS */
module components() {
  // PCB
  color([0.13,0.42,0.25]) translate([pcb_x,pcb_y,floor_th+standoff_h])
    linear_extrude(1.6) chamfered_pcb();
  z0 = pcb_top;
  // ESP32-S3 module (rear-left, antenna at rear edge)
  color([0.75,0.75,0.78]) translate([10,31.5,z0]) cube([18,25.5,3.2]);
  color([0.2,0.2,0.22])   translate([12,53,z0+0.4]) cube([14,3.5,1]);   // antenna zone
  // KS0835F lying flat on right-angle header
  color([0.1,0.1,0.12]) translate([30,20,z0+1.5]) cube([50.2,20.3,13]);
  // ES8388 codec + HT9170D
  color([0.2,0.2,0.2]) translate([15,14,z0]) cube([7,7,1.2]);
  color([0.2,0.2,0.2]) translate([64,13,z0]) cube([11,7,2]);
  // RJ11 jack (front, snout through wall)
  color([0.95,0.95,0.92]) translate([rj_cx-6.5,-1,floor_th+5.6]) cube([13,16,12]);
  // USB-C stub (rear)
  color([0.75,0.75,0.78]) translate([usb_cx-4.5,iy-7,z0]) cube([9,8.5,3.2]);
  // WS2812 LED
  color([0.9,0.85,0.4]) translate([led_cx-1,3.5,z0]) cube([2,2,1]);
}

/* ============================================================ ASSEMBLIES */
module shell_color() { color([0.93,0.88,0.78]) children(); }

if (part == "base") base();
else if (part == "lid") lid();
else if (part == "components") components();
else if (part == "display") { shell_color() base(); shell_color() translate([0,0,base_h]) lid(); }
else if (part == "interior") { shell_color() base(); components(); }
else if (part == "exploded") {
  shell_color() base(); components();
  shell_color() translate([0,0,base_h+40]) lid();
}
