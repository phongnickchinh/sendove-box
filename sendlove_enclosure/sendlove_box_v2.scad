// SendLove Box v2 - mat truoc toi thieu (chi man hinh), cac phan khac don sang hong va lung.
// Truc: x = rong (trai -> phai nhin tu truoc), y = sau (0 = mat truoc), z = cao (0 = day).
// Xuat:  openscad -D part=\"shell\" -o shell_v2.stl sendlove_box_v2.scad
//        openscad -D part=\"lid\"   -o lid_v2.stl   sendlove_box_v2.scad
// So ghi "GIA DINH" can do lai tren linh kien that.

part = "all";            // "all" | "shell" | "lid"
$fn = 40;

// ---------- Vo ----------
T = 2.0;                 // do day thanh
R = 3;                   // bo tron canh chay theo chieu sau
W = 33; H = 44; D = 63;  // kich thuoc ngoai (tinh ca nap sau)
lid_t = 2.0;
Ds = D - lid_t;          // chieu sau phan vo

// ---------- Man hinh ----------
mod_w = 27.7; mod_h = 39.1; mod_t = 4.4;
win = 25.6;
win_from_mod_top  = 10.0;                   // GIA DINH
win_from_mod_left = (mod_w - win) / 2;      // GIA DINH
win_clear = 0.3;
mod_hole_inset = 2.0;                       // GIA DINH
mod_post_h = 2.2;                           // GIA DINH
mod_post_d = 4.0; m16_pilot = 1.3;

// ---------- Pin 30 x 50 x 8, dung sat hong trai ----------
bat_t = 8; bat_h = 30; bat_l = 50;
bat_x0 = T + 0.5; bat_y0 = T + mod_t + 0.8; bat_z0 = T + 0.5;

// ---------- PCB dung song song hong trai (chua ve, chi canh vi tri) ----------
pcb_x0 = bat_x0 + bat_t + 1.0; pcb_t = 1.6; pcb_comp = 4.0;   // linh kien huong sang phai
// Mep sau PCB phai sat mat trong nap (cach <= 0.5 mm) de mieng cong USB-C va can gat cong tac ra toi nap.
pcb_y0 = bat_y0 + 1; pcb_l = Ds - 0.5 - pcb_y0; pcb_z0 = T + 1; pcb_h = H - 2 * T - 2;

// ---------- Loa 30 x 20 x 7.1, sat hong phai, phat ra hong phai ----------
spk_t = 7.1; spk_l = 30; spk_h = 20;
spk_y0 = 12; spk_z0 = (H - spk_h) / 2;
spk_rib = 1.2; spk_rib_h = 2.5; spk_clear = 0.4;

// ---------- Cam ung TTP223 tren noc, phia truoc ----------
touch_w = 8.8; touch_l = 12.7; touch_t = 0.4;
touch_y0 = 12; touch_wall = 1.0; touch_clear = 0.3;

// ---------- Mat lung: USB-C, cong tac, LED sac ----------
back_x = pcb_x0 + pcb_t;                    // mat phai PCB, noi han cac linh kien mep sau
usb_x = back_x + 1.6; usb_z = 12;           // USB-C dung doc (dai theo z)
usb_hole_l = 10.0; usb_hole_w = 4.2;
usb_plug_l = 12.5; usb_plug_w = 7.0; usb_plug_depth = 1.0;   // GIA DINH: hoc cho dau cap
d2_z = usb_z + 8.5; d2_d = 2.0;             // LED sac ngay tren cong USB-C
sw_x = back_x + 1.8; sw_z = 32;             // cong tac SS-12D00, gat theo z
sw_slot_l = 4.5; sw_slot_w = 2.0;
sw_body_l = 9.2; sw_body_w = 4.2; sw_wall = 1.0;

// ---------- LED tin nhan D3 o canh tren-truoc ----------
d3_d = 3.0; d3_x = W / 2;

// ---------- Vit nap sau (4 tru, tranh pin o goc duoi-trai) ----------
boss_d = 5.5; boss_len = 6; m2_pilot = 1.7;
lid_lip = 1.2; lid_lip_h = 2.0; lid_clear = 0.3;
bi = T + boss_d / 2 - 0.8;
boss_pos = [[bi, H - bi], [W - bi, H - bi], [W - bi, bi], [back_x + pcb_comp + 3.6, bi]];   // (x, z)

// ======================================================================

module rounded_block(w, h, d, r) {    // khoi chu nhat w (x) * h (z) * d (y), bo tron 4 canh chay theo y
    rotate([-90, 0, 0]) translate([0, -h, 0])
        hull() for (x = [r, w - r], z = [r, h - r]) translate([x, z, 0]) cylinder(r = r, h = d);
}

module slot(l, w, h) { hull() for (s = [-1, 1]) translate([s * (l - w) / 2, 0, 0]) cylinder(d = w, h = h); }

mod_x0 = (W - mod_w) / 2;
mod_ztop = (H + mod_h) / 2;
win_x0 = mod_x0 + win_from_mod_left;
win_ztop = mod_ztop - win_from_mod_top;

module shell() {
    difference() {
        union() {
            difference() {
                rounded_block(W, H, Ds, R);
                translate([T, T, T]) cube([W - 2 * T, Ds, H - 2 * T]);
            }
            for (p = boss_pos) translate([p[0], Ds - boss_len, p[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d, h = boss_len);
            // tru man hinh, cat phan lan vao vung kinh
            difference() {
                for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
                    translate([mod_x0 + dx, T, mod_ztop - dz]) rotate([-90, 0, 0]) cylinder(d = mod_post_d, h = mod_post_h);
                translate([win_x0 - 1, 0, win_ztop - win - 1]) cube([win + 2, T + mod_post_h + 1, win + 2]);
            }
            // khung giu loa tren hong phai
            translate([W - T - spk_rib_h, spk_y0 - spk_clear - spk_rib, spk_z0 - spk_clear - spk_rib])
                difference() {
                    cube([spk_rib_h, spk_l + 2 * (spk_clear + spk_rib), spk_h + 2 * (spk_clear + spk_rib)]);
                    translate([-1, spk_rib, spk_rib]) cube([spk_rib_h + 2, spk_l + 2 * spk_clear, spk_h + 2 * spk_clear]);
                }
            // go chan pin (giu pin sat hong trai)
            translate([bat_x0 + bat_t + 0.3, bat_y0 + 8, T]) cube([1.2, bat_l - 16, 3]);
        }
        for (p = boss_pos) translate([p[0], Ds - boss_len + 1, p[1]]) rotate([-90, 0, 0]) cylinder(d = m2_pilot, h = boss_len + 1);
        for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
            translate([mod_x0 + dx, T + 0.6, mod_ztop - dz]) rotate([-90, 0, 0]) cylinder(d = m16_pilot, h = mod_post_h + 1);
        // cua so man hinh, vat mep ra ngoai
        translate([win_x0 - win_clear, -0.01, win_ztop - win - win_clear])
            hull() {
                translate([0, T - 0.6, 0]) cube([win + 2 * win_clear, 0.62, win + 2 * win_clear]);
                translate([-0.8, 0, -0.8]) cube([win + 2 * win_clear + 1.6, 0.01, win + 2 * win_clear + 1.6]);
            }
        // luoi loa tren hong phai: khe doc
        for (i = [0 : 7]) translate([W - T - 1, spk_y0 + 3 + i * 3.4, spk_z0 + 3]) hull() {
            translate([0, 0.8, 0.8]) rotate([0, 90, 0]) cylinder(d = 1.6, h = T + 2);
            translate([0, 0.8, spk_h - 6.8]) rotate([0, 90, 0]) cylinder(d = 1.6, h = T + 2);
        }
        // hoc cam ung o noc
        translate([W / 2 - touch_w / 2 - touch_clear, touch_y0 - touch_clear, H - T - 0.01])
            cube([touch_w + 2 * touch_clear, touch_l + 2 * touch_clear, T - touch_wall + 0.01]);
        // LED tin nhan D3: lo xien 45 do o canh tren-truoc
        translate([d3_x, 1.2, H - 1.2]) rotate([45, 0, 0]) translate([0, 0, -6]) cylinder(d = d3_d, h = 12);
    }
}

module lid() {
    difference() {
        union() {
            translate([0, Ds, 0]) rounded_block(W, H, lid_t, R);
            translate([T + lid_clear, Ds - lid_lip_h, T + lid_clear]) difference() {
                cube([W - 2 * (T + lid_clear), lid_lip_h, H - 2 * (T + lid_clear)]);
                translate([lid_lip, -1, lid_lip]) cube([W - 2 * (T + lid_clear + lid_lip), lid_lip_h + 2, H - 2 * (T + lid_clear + lid_lip)]);
            }
        }
        for (p = boss_pos) {
            translate([p[0], Ds - lid_lip_h - 1, p[1]]) rotate([-90, 0, 0]) cylinder(d = 2.4, h = lid_t + lid_lip_h + 2);
            translate([p[0], D - 1.2, p[1]]) rotate([-90, 0, 0]) cylinder(d1 = 2.4, d2 = 4.4, h = 1.21);
            translate([p[0], Ds - lid_lip_h - 0.5, p[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d + 0.8, h = lid_lip_h + 0.5);
        }
        // USB-C doc + hoc dau cap
        translate([usb_x, Ds - 1, usb_z]) rotate([-90, 0, 0]) rotate([0, 0, 90]) slot(usb_hole_l, usb_hole_w, lid_t + 2);
        translate([usb_x, D - usb_plug_depth, usb_z]) rotate([-90, 0, 0]) rotate([0, 0, 90]) slot(usb_plug_l, usb_plug_w, usb_plug_depth + 1);
        // LED sac
        translate([usb_x, Ds - 1, d2_z]) rotate([-90, 0, 0]) cylinder(d = d2_d, h = lid_t + 2);
        // cong tac: khe can gat + lam mong nap quanh than
        translate([sw_x, Ds - 1, sw_z]) rotate([-90, 0, 0]) rotate([0, 0, 90]) slot(sw_slot_l, sw_slot_w, lid_t + 2);
        translate([sw_x - sw_body_w / 2, Ds - lid_lip_h - 0.01, sw_z - sw_body_l / 2]) cube([sw_body_w, lid_lip_h + lid_t - sw_wall, sw_body_l]);
    }
}

module ghosts() {
    color("DimGray", 0.8)   translate([mod_x0, T, mod_ztop - mod_h]) cube([mod_w, mod_t, mod_h]);
    color("Turquoise")      translate([win_x0, T - 0.05, win_ztop - win]) cube([win, 0.1, win]);
    color("RoyalBlue", 0.8) translate([bat_x0, bat_y0, bat_z0]) cube([bat_t, bat_l, bat_h]);
    color("SeaGreen", 0.8)  translate([pcb_x0, pcb_y0, pcb_z0]) cube([pcb_t, pcb_l, pcb_h]);
    color("Orange", 0.8)    translate([W - T - spk_t, spk_y0, spk_z0]) cube([spk_t, spk_l, spk_h]);
    color("HotPink")        translate([W / 2 - touch_w / 2, touch_y0, H - T - touch_t]) cube([touch_w, touch_l, touch_t]);
    color("Red", 0.5)       translate([pcb_x0 + pcb_t, pcb_y0 + pcb_l - 13, pcb_z0 + pcb_h - 6]) cube([2.4, 13, 6]);   // vung anten ESP32
}

if (part == "shell") shell();
else if (part == "lid") lid();
else {
    color("WhiteSmoke") shell();
    color("LightSteelBlue", 0.9) translate([0, 8, 0]) lid();
    ghosts();
}
