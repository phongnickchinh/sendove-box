// SendLove Box - vo hop mau (prototype), kieu BMO.
// Don vi: mm. Truc: x = chieu rong (trai -> phai, nhin tu truoc),
// y = chieu sau (0 = mat truoc, D = mat sau), z = chieu cao (0 = day).
// Xuat tung phan:  openscad -D part=\"shell\" -o shell.stl sendlove_box.scad
//                  openscad -D part=\"lid\"   -o lid.stl   sendlove_box.scad
// Cac so co ghi "GIA DINH" can do lai tren linh kien that.

part = "all";          // "all" (xem lap rap) | "shell" | "lid"

// ---------- Vo ----------
W = 50; H = 76; D = 32;     // kich thuoc ngoai
T = 2.0;                    // do day thanh
R = 3;                      // bo tron canh dung
$fn = 40;

// ---------- Man hinh (module ST7789) ----------
mod_w = 27.7; mod_h = 39.1; mod_t = 4.4;
mod_top_gap = 5;                          // tu mep tren hop toi mep tren module
win = 25.6;                               // vung hien thi
win_from_mod_top  = 10.0;                 // GIA DINH: mep tren module -> mep tren vung hien thi
win_from_mod_left = (mod_w - win) / 2;    // GIA DINH: nam giua theo chieu ngang
win_clear = 0.3;                          // du thua moi canh cua so
mod_hole_d = 1.8;                         // lo vit module (vit M1.6)
mod_hole_inset = 2.0;                     // GIA DINH: tam lo cach mep module
mod_post_h = 2.2;                         // GIA DINH: tu mat kinh toi mat truoc bo module
mod_post_d = 4.0;
m16_pilot = 1.3;                          // lo moi cho vit tu ren M1.6

// ---------- Loa 30 x 20 x 7.1 ----------
spk_w = 30; spk_h = 20; spk_t = 7.1;
spk_z0 = 3.0;                             // day loa cach day hop (ngoai)
spk_rib = 1.2; spk_rib_h = 3; spk_clear = 0.4;

// ---------- Pin 30 x 50 x 8 ----------
bat_w = 30; bat_h = 50; bat_t = 8;

// ---------- PCB (chua ve, chi de canh vi tri) ----------
pcb_y0 = 10.0; pcb_t = 1.6; pcb_comp = 4.0;

// ---------- Cam ung TTP223 (mat cam ung huong len) ----------
touch_w = 8.8; touch_l = 12.7; touch_t = 0.4;
touch_y0 = 12;                            // cach mat truoc
touch_wall = 1.0;                         // thanh tren con lai tai cho cam ung
touch_clear = 0.3;

// ---------- USB-C (hong phai) ----------
usb_z = 16;                               // tam cong, cach day
usb_y = pcb_y0 + pcb_t + 1.6;             // tam cong theo chieu sau (cong han mat sau PCB)
usb_hole_l = 10.0; usb_hole_w = 4.2;      // lo xuyen cho vo cong (~9 x 3.2)
usb_plug_l = 12.5; usb_plug_w = 7.0;      // GIA DINH: hoc cho phan nhua boc dau cap
usb_plug_depth = 1.0;

// ---------- LED sac D2 (L1: canh cong USB-C) ----------
d2_z = usb_z + 8; d2_d = 2.0;

// ---------- Cong tac SS-12D00 (hong trai) ----------
sw_z = 46;
sw_y = pcb_y0 + pcb_t + 1.8;
sw_slot_l = 4.5; sw_slot_w = 2.0;         // khe cho can gat (huong gat theo z)
sw_body_l = 9.2; sw_body_w = 4.2;         // hoc lam mong thanh quanh than cong tac
sw_wall = 1.0;                            // thanh con lai tai cho cong tac

// ---------- LED tin nhan D3 (canh tren-truoc) ----------
d3_d = 3.0;

// ---------- Vit nap sau ----------
boss_d = 5.5; boss_len = 8; m2_pilot = 1.7;   // vit tu ren M2 (hoac oc cay nong: doi m2_pilot = 3.2)
lid_t = 2.0; lid_lip = 1.2; lid_lip_h = 2.0; lid_clear = 0.3;
boss_inset = T + boss_d / 2 - 0.8;      // tru lan vao goc thanh de dinh lien vo
Ds = D - lid_t;                           // chieu sau phan vo (nap nam ngoai)

// ---------- Trang tri BMO ----------
deco_h = 0.6;

// ======================================================================

module rounded_box(w, d, h, r) {
    hull() for (x = [r, w - r], y = [r, d - r]) translate([x, y, 0]) cylinder(r = r, h = h);
}

// Toa do mat truoc: (x, z_tu_tren) -> z
function ztop(y_from_top) = H - y_from_top;

mod_x0 = (W - mod_w) / 2;
mod_ztop = H - mod_top_gap;
win_x0 = mod_x0 + win_from_mod_left;
win_ztop = mod_ztop - win_from_mod_top;
spk_x0 = (W - spk_w) / 2;
bat_x0 = W - T - bat_w - 0.5;
bat_y0 = pcb_y0 + pcb_t + pcb_comp + 1.0;
bat_z0 = 8;

boss_pos = [[boss_inset, boss_inset], [W - boss_inset, boss_inset],
            [boss_inset, H - boss_inset], [W - boss_inset, H - boss_inset]];   // (x, z)

module shell() {
    difference() {
        union() {
            difference() {
                // than hop: hop kin tru long ben trong, ho mat sau
                rotate([90, 0, 0]) translate([0, 0, -Ds]) rounded_box(W, H, Ds, R);   // (x, z) mat cat, dai theo y
                translate([T, T, T]) cube([W - 2 * T, D, H - 2 * T]);
            }
            // tru bat vit nap sau
            for (p = boss_pos) translate([p[0], Ds - boss_len, p[1]])
                rotate([-90, 0, 0]) cylinder(d = boss_d, h = boss_len);
            // tru bat module man hinh (4 goc), cat bo phan lan vao vung kinh man hinh
            difference() {
                for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
                    translate([mod_x0 + dx, T, mod_ztop - dz]) rotate([-90, 0, 0]) cylinder(d = mod_post_d, h = mod_post_h);
                translate([win_x0 - 1, 0, win_ztop - win - 1]) cube([win + 2, T + mod_post_h + 1, win + 2]);
            }
            // go giu loa
            translate([spk_x0 - spk_clear - spk_rib, T, spk_z0 - spk_clear - spk_rib])
                difference() {
                    cube([spk_w + 2 * (spk_clear + spk_rib), spk_rib_h, spk_h + 2 * (spk_clear + spk_rib)]);
                    translate([spk_rib, -1, spk_rib]) cube([spk_w + 2 * spk_clear, spk_rib_h + 2, spk_h + 2 * spk_clear]);
                }
            // trang tri BMO (noi)
            translate([0, 0, 0]) bmo_deco();
        }
        // lo vit trong tru nap sau
        for (p = boss_pos) translate([p[0], Ds - boss_len + 1, p[1]]) rotate([-90, 0, 0]) cylinder(d = m2_pilot, h = boss_len + 1);
        // lo vit module man hinh
        for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
            translate([mod_x0 + dx, T + 0.6, mod_ztop - dz]) rotate([-90, 0, 0]) cylinder(d = m16_pilot, h = mod_post_h + 1);
        // cua so man hinh (vat mep 45 do ra ngoai)
        translate([win_x0 - win_clear, -0.01, win_ztop - win - win_clear])
            hull() {
                translate([0, T - 0.6, 0]) cube([win + 2 * win_clear, 0.62, win + 2 * win_clear]);
                translate([-0.8, 0, -0.8]) cube([win + 2 * win_clear + 1.6, 0.01, win + 2 * win_clear + 1.6]);
            }
        // khe thoat am loa
        for (i = [0 : 6]) translate([15.6 + i * 3.0, -1, spk_z0 + 2]) hull() {
            translate([0.8, 0, 0.8]) rotate([-90, 0, 0]) cylinder(d = 1.6, h = T + 2);
            translate([0.8, 0, 7.2]) rotate([-90, 0, 0]) cylinder(d = 1.6, h = T + 2);
        }
        // khe ngang trang tri (khac chim)
        translate([12, -0.01, ztop(47) - 1.4]) cube([18, deco_h, 1.4]);
        // hoc cam ung o thanh tren
        translate([W / 2 - touch_w / 2 - touch_clear, touch_y0 - touch_clear, H - T - 0.01])
            cube([touch_w + 2 * touch_clear, touch_l + 2 * touch_clear, T - touch_wall + 0.01]);
        // USB-C: lo xuyen + hoc cho dau cap o mat ngoai
        translate([W - T - 1, usb_y, usb_z]) rotate([0, 90, 0]) slot(usb_hole_l, usb_hole_w, T + 2);
        translate([W - usb_plug_depth, usb_y, usb_z]) rotate([0, 90, 0]) slot(usb_plug_l, usb_plug_w, usb_plug_depth + 1);
        // LED sac D2 canh cong USB-C
        translate([W - T - 1, usb_y, d2_z]) rotate([0, 90, 0]) cylinder(d = d2_d, h = T + 2);
        // cong tac: khe can gat + lam mong thanh quanh than
        translate([-1, sw_y, sw_z]) rotate([0, 90, 0]) slot(sw_slot_l, sw_slot_w, T + 2);
        translate([sw_wall, sw_y - sw_body_w / 2, sw_z - sw_body_l / 2]) cube([T, sw_body_w, sw_body_l]);
        // LED tin nhan D3: lo xien 45 do o canh tren-truoc
        translate([W / 2, 1.2, H - 1.2]) rotate([45, 0, 0]) translate([0, 0, -6]) cylinder(d = d3_d, h = 12);
    }
}

// hinh vien thuoc (2 dau tron): dai l theo truc x cuc bo (sau khi xoay = theo z the gioi), rong w
module slot(l, w, h) {
    hull() for (s = [-1, 1]) translate([s * (l - w) / 2, 0, 0]) cylinder(d = w, h = h);
}

module bmo_deco() {
    // D-pad
    translate([9.5, -deco_h, ztop(58.5)]) cube([9, deco_h, 3]);
    translate([12.5, -deco_h, ztop(61.5)]) cube([3, deco_h, 9]);
    // tam giac
    translate([36, 0, ztop(57)]) rotate([90, 0, 0]) linear_extrude(deco_h)
        polygon([[-2.5, 0], [2.5, 0], [0, 4]]);
    // nut do lon, nut xanh nho
    translate([40.5, 0, ztop(60)]) rotate([90, 0, 0]) cylinder(r = 2.2, h = deco_h);
    translate([34.5, 0, ztop(62)]) rotate([90, 0, 0]) cylinder(r = 1.4, h = deco_h);
}

module lid() {
    difference() {
        union() {
            translate([0, Ds, 0]) rotate([90, 0, 0]) translate([0, 0, -lid_t]) rounded_box(W, H, lid_t, R);
            // go dinh vi lot vao trong mieng hop
            translate([T + lid_clear, Ds - lid_lip_h, T + lid_clear])
                difference() {
                    cube([W - 2 * (T + lid_clear), lid_lip_h, H - 2 * (T + lid_clear)]);
                    translate([lid_lip, -1, lid_lip]) cube([W - 2 * (T + lid_clear + lid_lip), lid_lip_h + 2, H - 2 * (T + lid_clear + lid_lip)]);
                }
        }
        // lo vit M2 + loe dau vit
        for (p = boss_pos) {
            translate([p[0], Ds - lid_lip_h - 1, p[1]]) rotate([-90, 0, 0]) cylinder(d = 2.4, h = lid_t + lid_lip_h + 2);
            translate([p[0], D - 1.2, p[1]]) rotate([-90, 0, 0]) cylinder(d1 = 2.4, d2 = 4.4, h = 1.21);
            // khoet go lot cho tru vit
            translate([p[0], Ds - lid_lip_h - 0.5, p[1]]) rotate([-90, 0, 0]) cylinder(d = boss_d + 0.8, h = lid_lip_h + 0.5);
        }
    }
}

// Linh kien ma (chi de xem lap rap, khong xuat STL)
module ghosts() {
    color("DimGray", 0.8)  translate([mod_x0, T, mod_ztop - mod_h]) cube([mod_w, mod_t, mod_h]);
    color("Turquoise")     translate([win_x0, T - 0.05, win_ztop - win]) cube([win, 0.1, win]);
    color("Orange", 0.8)   translate([spk_x0, T, spk_z0]) cube([spk_w, spk_t, spk_h]);
    color("SeaGreen", 0.8) translate([T + 0.5, pcb_y0, T + 3]) cube([W - 2 * T - 1, pcb_t, 66]);
    color("RoyalBlue", 0.8) translate([bat_x0, bat_y0, bat_z0]) cube([bat_w, bat_t, bat_h]);
    color("HotPink")       translate([W / 2 - touch_w / 2, touch_y0, H - T - touch_t]) cube([touch_w, touch_l, touch_t]);
}

if (part == "shell") shell();
else if (part == "lid") lid();
else {
    color("WhiteSmoke") shell();
    color("LightSteelBlue", 0.9) translate([0, 6, 0]) lid();   // nap keo lui 6 mm de nhin
    ghosts();
}
