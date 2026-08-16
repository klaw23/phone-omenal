// ============================================================
//  OPEN PHONE CO. — "Model 01" enclosure
//  Open-source enclosure for an ESP32 SIP phone box:
//    - Ai-Thinker ESP32-A1S Audio Kit (~90 x 60 mm)  [measure yours!]
//    - 60x40 perfboard carrying KS0835F SLIC + MT8870 + level shifter
//    - RJ11 keystone jack on the front
//  License: CERN-OHL-S / CC-BY-SA — do whatever, share alike.
//
//  part = "base" | "lid" | "components" | "display" | "exploded" | "interior"
// ============================================================

part = "display";

/* ---------- main parameters (mm) ---------- */
wall      = 2.4;      // wall thickness
floor_th  = 2.4;      // floor thickness
lid_th    = 2.8;      // lid plate thickness
corner_r  = 6;        // outer corner radius

ix = 140;             // interior width  (x)
iy = 96;              // interior depth  (y)
iz = 34;              // interior height (z, above floor)

// ESP32 Audio Kit bay (board + tolerance)
audio_w = 90;  audio_d = 60;  audio_standoff = 5;

// perfboard (common 60x40, mounting holes ~55x35, M2)
perf_w = 40; perf_d = 60; perf_hole_dx = 35; perf_hole_dy = 55;
perf_cx = 118; perf_cy = 54; perf_post_h = 6;

// RJ11 keystone cutout (standard keystone: 14.5 x 16.0)
key_w = 14.7; key_h = 16.1; key_cx = 30; key_z = 6;

// status LED hole (5mm LED)
led_d = 5.4; led_cx = 70; led_z = 14;

// rear connector slot for AudioKit edge (USB x2 + jacks)
slot_w = 76; slot_cx = 49; slot_z0 = 3; slot_z1 = 17;

// lid screws (M3 self-tap into posts)
post_r = 4.6; screw_pilot = 2.7; screw_head = 6.4; screw_shaft = 3.4;

// lid vent: rotary-dial pattern
dial_cx = 49; dial_cy = 52; dial_r = 22; dial_hole_d = 9; dial_n = 10;

label_text = "OPEN PHONE CO.";
label_sub  = "MODEL 01";

/* ---------- derived ---------- */
ox = ix + 2*wall;         // outer footprint
oy = iy + 2*wall;
base_h = floor_th + iz;   // base outer height
$fn = 48;
// screw post centers, sunk 0.8mm into the walls so the union is clean
pin = post_r - 0.8;
post_pts = [[pin,pin],[ix-pin,pin],[pin,iy-pin],[ix-pin,iy-pin]];

/* ---------- helpers ---------- */
module rrect(w, d, r) {
  hull() for (x=[r, w-r], y=[r, d-r]) translate([x,y]) circle(r=r);
}
module rbox(w, d, h, r) { linear_extrude(h) rrect(w, d, r); }

/* ============================================================
   BASE
   ============================================================ */
module base() {
  difference() {
    union() {
      // outer shell
      difference() {
        translate([-wall,-wall,0]) rbox(ox, oy, base_h, corner_r);
        translate([0,0,floor_th]) rbox(ix, iy, iz+1, corner_r-wall);
      }
      // corner screw posts
      for (p=post_pts)
        translate([p[0],p[1],floor_th]) cylinder(r=post_r, h=iz);
      // AudioKit corner grips: ledge + lip at each board corner
      audiokit_grips();
      // perfboard posts (M2)
      for (dx=[-1,1], dy=[-1,1])
        translate([perf_cx+dx*perf_hole_dx/2, perf_cy+dy*perf_hole_dy/2, floor_th])
          difference() {
            cylinder(r=3.2, h=perf_post_h);
            translate([0,0,perf_post_h-5]) cylinder(r=0.9, h=5.1);
          }
      // keystone tunnel reinforcement on front wall
      translate([key_cx-key_w/2-2.5, -0.5, floor_th])
        cube([key_w+5, 3.5, key_z+key_h+2.5]);
    }
    // screw pilot holes
    for (p=post_pts)
      translate([p[0],p[1],floor_th+iz-12]) cylinder(r=screw_pilot/2, h=12.1);
    // keystone cutout (front wall, y = -wall..3+)
    translate([key_cx-key_w/2, -wall-0.5, floor_th+key_z])
      cube([key_w, wall+4.5, key_h]);
    // LED hole
    translate([led_cx, 0.5, floor_th+led_z])
      rotate([90,0,0]) cylinder(d=led_d, h=wall+1.5);
    // rear connector slot
    translate([slot_cx-slot_w/2, iy-0.5, floor_th+slot_z0])
      cube([slot_w, wall+1, slot_z1-slot_z0]);
    // engrave "LINE" beside the jack
    translate([key_cx+key_w/2+4, -wall+0.5, floor_th+key_z+4]) rotate([90,0,0])
      linear_extrude(1) text("LINE", size=3.4, halign="left", font="Liberation Sans:style=Bold");
  }
}

// one grip drawn for a (+x,+y) corner at origin, mirrored into place
module grip_pp() {
  cube([8, 8, audio_standoff]);                          // ledge the board rests on
  translate([-2.5,-2.5,0]) cube([11, 2.5, audio_standoff+5]);  // outer lip, L-shaped
  translate([-2.5,-2.5,0]) cube([2.5, 11, audio_standoff+5]);  // (overlapping corner)
}
module audiokit_grips() {
  bx = 4; by = iy - 2 - audio_d;   // board front-left corner
  translate([bx,          by,          floor_th]) scale([ 1, 1,1]) grip_pp();
  translate([bx+audio_w,  by,          floor_th]) scale([-1, 1,1]) grip_pp();
  translate([bx,          by+audio_d,  floor_th]) scale([ 1,-1,1]) grip_pp();
  translate([bx+audio_w,  by+audio_d,  floor_th]) scale([-1,-1,1]) grip_pp();
}

/* ============================================================
   LID
   ============================================================ */
module lid() {
  difference() {
    union() {
      translate([-wall,-wall,0]) rbox(ox, oy, lid_th, corner_r);
      // alignment tabs (skirt segments along edge midspans, clear of posts)
      tab_l = 42; tab_t = 1.6; tab_h = 3.2; inset = 0.3;
      for (s = [ [ix/2-tab_l/2, inset, tab_l, tab_t],
                 [ix/2-tab_l/2, iy-inset-tab_t, tab_l, tab_t] ])
        translate([s[0], s[1], -tab_h]) cube([s[2], s[3], tab_h]);
      for (s = [ [inset, iy/2-tab_l/2, tab_t, tab_l],
                 [ix-inset-tab_t, iy/2-tab_l/2, tab_t, tab_l] ])
        translate([s[0], s[1], -tab_h]) cube([s[2], s[3], tab_h]);
    }
    // countersunk screw holes over posts
    for (p=post_pts) {
      translate([p[0],p[1],-0.1]) cylinder(d=screw_shaft, h=lid_th+0.2);
      translate([p[0],p[1],lid_th-1.7]) cylinder(d1=screw_shaft, d2=screw_head, h=1.71);
    }
    // rotary-dial vent holes
    for (i=[0:dial_n-1]) {
      a = 90 + 30 + i*(300/(dial_n-1));   // arc like a real dial (gap at bottom)
      translate([dial_cx + dial_r*cos(a), dial_cy + dial_r*sin(a), -0.1])
        cylinder(d=dial_hole_d, h=lid_th+0.2);
    }
    // center hole of the "dial"
    translate([dial_cx, dial_cy, -0.1]) cylinder(d=12, h=lid_th+0.2);
    // engraved dial ring
    translate([dial_cx, dial_cy, lid_th-0.6]) difference() {
      cylinder(r=dial_r+dial_hole_d/2+2.2, h=0.7);
      translate([0,0,-0.1]) cylinder(r=dial_r+dial_hole_d/2+1.0, h=0.9);
    }
    // engraved labels (right-aligned block, clear of the dial)
    translate([132, 62, lid_th-0.6]) linear_extrude(0.7)
      text(label_text, size=5.6, halign="right", font="Liberation Sans:style=Bold");
    translate([132, 51, lid_th-0.6]) linear_extrude(0.7)
      text(label_sub, size=4.0, halign="right", font="Liberation Sans");
    translate([132, 32, lid_th-0.6]) linear_extrude(0.7)
      text("an open source telephone", size=3.2, halign="right", font="Liberation Sans:style=Italic");
  }
}

/* ============================================================
   COMPONENT MOCKUPS (for visualization only)
   ============================================================ */
module components() {
  bx = 4; by = iy - 2 - audio_d;
  // ESP32 Audio Kit
  color([0.13,0.4,0.2]) translate([bx,by,floor_th+audio_standoff]) cube([audio_w,audio_d,1.6]);
  color([0.25,0.25,0.28]) translate([bx+30,by+38,floor_th+audio_standoff+1.6]) cube([28,17,3.2]); // ESP32-A1S module
  color([0.75,0.75,0.78]) { // USB stubs through rear slot
    translate([bx+18,by+audio_d-3,floor_th+audio_standoff+1.6]) cube([8,7,3]);
    translate([bx+38,by+audio_d-3,floor_th+audio_standoff+1.6]) cube([8,7,3]);
  }
  color([0.1,0.1,0.1]) translate([bx+62,by+audio_d-5,floor_th+audio_standoff+1.6]) cube([7,9,5]); // headphone jack
  // perfboard
  color([0.82,0.7,0.45]) translate([perf_cx-perf_w/2, perf_cy-perf_d/2, floor_th+perf_post_h]) cube([perf_w,perf_d,1.6]);
  // KS0835F SLIC (SIL stick standing up)
  color([0.1,0.1,0.12]) translate([perf_cx-6.5, perf_cy-25, floor_th+perf_post_h+1.6]) cube([13,50.2,20.3]);
  // MT8870 module
  color([0.15,0.3,0.6]) translate([perf_cx-11, perf_cy+28-52, floor_th+perf_post_h+1.6+0]) ;
  color([0.15,0.3,0.6]) translate([perf_cx+8, perf_cy-21, floor_th+perf_post_h+1.6]) cube([9,42,10]);
  // keystone jack body
  color([0.95,0.95,0.92]) translate([key_cx-8, 0, floor_th+key_z-2]) cube([16,22,key_h+4]);
}

/* ============================================================
   ASSEMBLIES
   ============================================================ */
module shell_color() { color([0.93,0.88,0.78]) children(); }  // warm cream PLA

if (part == "base")        base();
else if (part == "lid")    lid();
else if (part == "components") components();
else if (part == "display") {
  shell_color() base();
  shell_color() translate([0,0,base_h]) lid();
}
else if (part == "interior") {
  shell_color() base();
  components();
}
else if (part == "exploded") {
  shell_color() base();
  components();
  shell_color() translate([0,0,base_h+45]) lid();
}
