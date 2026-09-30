// SendLove Box v4 - con meo nam "o banh mi". Mat nghieng chua man hinh, 2 tai in rieng chua LED.
// Truc: x = ngang (-: trai, +: phai khi nhin mat meo), y = doc than (0 = mui/mat, tang ve phia duoi),
//       z = cao (0 = mat ban).
// Xuat:  openscad -D part=\"body\" -o cat_body.stl sendlove_cat_v4.scad
//        openscad -D part=\"base\" -o cat_base.stl sendlove_cat_v4.scad
//        openscad -D part=\"ear\"  -o cat_ear.stl  sendlove_cat_v4.scad   (in 2 cai, nhua trang/mo)
// Kiem tra va cham: openscad -D part=\"check\" -o check.stl ...  -> phai bao "object is empty".
// So ghi "GIA DINH" can do lai tren linh kien that.

part = "all";          // all | body | base | ear | check
$fn = 48;

// ---------- Than o banh mi ----------
T   = 2.0;              // do day thanh
W   = 44;               // be ngang
L   = 92;               // be dai (toi da, truoc khi cat)
Rd  = W / 2;            // ban kinh lung tron
zc  = 28;               // tam cung lung  -> cao H = zc + Rd = 50
H   = zc + Rd;
y1  = 18; y2 = L - 22;  // tam 2 dau vien lung
rb  = 12;               // bo goc day
y_rear = 86;            // mat phang sau (mong), noi dat cong USB-C + cong tac

// ---------- Mat (man hinh) ----------
tilt = 35;              // mat nga ve sau so voi phuong dung (do)
yf0 = 4; zf0 = 5;       // diem canh duoi mat
ta = tan(tilt); ca = cos(tilt);
mod_w = 27.7; mod_h = 39.1; mod_t = 4.4;
mod_v0 = 3.0;           // mep duoi module cach canh duoi mat (doc mat xien)
win = 25.6;
win_from_mod_top  = 10.0;                 // GIA DINH
win_from_mod_left = (mod_w - win) / 2;    // GIA DINH
win_clear = 0.3;
mod_hole_inset = 2.0;                     // GIA DINH
mod_post_h = 2.2;                         // GIA DINH
mod_post_d = 4.0; m16_pilot = 1.3;

// ---------- Tai ----------
ear_x = 11; ear_y = 40;                   // vi tri tam tai
ear_peg_d = 5.6; ear_hole_d = 5.9; ear_peg_len = 4;
ear_w = 15; ear_d = 7.5; ear_h = 13; ear_wall = 1.3;   // tai det: day rong, mong (vua LED 3 mm)
ear_tilt = 20;                                         // tai nga ra ngoai (do)
function dome_z(x) = zc + sqrt(Rd * Rd - x * x);
// he toa do goc tai s (-1 trai, +1 phai): goc tai tai mat lung, truc tai nga ra ngoai
module ear_frame(s) { translate([s * ear_x, ear_y, dome_z(ear_x) - 0.4]) rotate([0, s * ear_tilt, 0]) children(); }

// ---------- Linh kien ben trong ----------
bat = [30, 50, 8];      // pin nam phang tren tam day, dat lech trai
bat_pos = [-18, 23.5, T + 0.5];
spk = [7.1, 30, 20];    // loa dung sat hong phai, ha thap (lung cong xuong o sat hong)
spk_pos = [W / 2 - T - 7.1 - 0.2, 34, 3];
pcb = [26, 64, 1.6];    // PCB nam ngang tren pin (chua ve): 26 x 64 mm
pcb_pos = [-14, 18.5, 12];
pcb_comp = 5;           // linh kien mat tren PCB (3 mm dau bo phia truoc phai de trong: sat lung man hinh)
pcb_front_free = 3.5;
standoffs = [[-11, 20.5], [8, 20.5], [-10, 78.5], [6, 78.5]];   // tru do PCB: 2 truoc pin, 2 sau pin
touch = [8.8, 12.7, 0.4]; touch_y = 55; touch_wall = 1.0; touch_clear = 0.3;

// ---------- Mong: USB-C, cong tac, LED sac ----------
usb_x = -4;  usb_z = pcb_pos[2] + 1.6 + 1.6;
usb_hole = [10, 4.2]; usb_plug = [12.5, 7]; usb_plug_depth = 1.0;   // GIA DINH
d2_x = usb_x - 9; d2_d = 2.0;
sw_x = 8; sw_z = pcb_pos[2] + 1.6 + 1.8; sw_slot = [4.5, 2.0];

// ---------- Tam day + vit ----------
base_t = 2.0; base_clear = 0.3;
boss_d = 6; boss_h = 8; m2_pilot = 1.7;
bosses = [[-17.5, 11], [17.5, 11], [-17.5, 82], [17.5, 82]];   // tru sau lan vao thanh mong

// ======================================================================
// Hinh khoi co ban
module loaf(inset) {
    r = Rd - inset;
    hull() {
        for (y = [y1, y2]) translate([0, y, zc]) sphere(r = r);
        translate([0, 0, -1]) linear_extrude(1 + 0.01) offset(r = rb - inset) offset(delta = -rb)
            translate([-W / 2, 2, 0]) square([W, L - 4]);
    }
}
// Nua khong gian sau mat phang mat meo, lui vao `off`
module on_face() { translate([0, yf0, zf0]) rotate([-tilt, 0, 0]) children(); }
module behind_face(off) { on_face() translate([-100, off, -100]) cube([200, 300, 300]); }
module before_rear(off) { translate([-100, -100, -100]) cube([200, y_rear - off + 100, 300]); }

module outer_solid() { intersection() { loaf(0); behind_face(0); before_rear(0); } }
module inner_void() {
    intersection() { loaf(T); behind_face(T); before_rear(T); }
    translate([-W / 2 + T, 0, -5]) cube([W - 2 * T, y_rear, 5 + T]);   // mo day
}

mod_vtop = mod_v0 + mod_h;
win_vtop = mod_vtop - win_from_mod_top;
win_x0 = -win / 2;

module paws() {
    for (s = [-1, 1]) translate([s * 9, 3, 3.6]) scale([6, 7, 3.5]) sphere(r = 1);
}
module tail() {
    pts = [[-W / 2 + 1, 84, 4], [-W / 2 + 1, 70, 3.3], [-W / 2 + 1, 52, 3.6], [-W / 2 + 1, 36, 6], [-W / 2 + 1.5, 26, 11], [-W / 2 + 2.5, 22, 17]];
    for (i = [0 : len(pts) - 2]) hull() { translate(pts[i]) sphere(r = 3); translate(pts[i + 1]) sphere(r = 3); }
}

module body() {
    difference() {
        union() {
            // ghep chan + duoi vao vo ngoai TRUOC, roi moi khoet long (khong de chan lan vao ben trong)
            difference() { union() { outer_solid(); paws(); tail(); } inner_void(); }
            // tru bat tam day (lan vao giua thanh, tranh mat trung voi mat ngoai)
            for (b = bosses) intersection() {
                translate([b[0], b[1], T]) cylinder(d = boss_d, h = boss_h);
                loaf(T / 2);
            }
            // tru man hinh tren mat, cat phan lan vao vung kinh
            on_face() difference() {
                for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
                    translate([-mod_w / 2 + dx, T, mod_v0 + dz]) rotate([-90, 0, 0]) cylinder(d = mod_post_d, h = mod_post_h);
                translate([win_x0 - 1, 0, win_vtop - win - 1]) cube([win + 2, T + mod_post_h + 1, win + 2]);
            }
        }
        // be tai: mat phang nong vuong goc truc tai + lo chot (luon day LED)
        for (s = [-1, 1]) ear_frame(s) {
            scale([ear_w / 2 + 0.4, ear_d / 2 + 0.4, 1]) cylinder(r = 1, h = 8);
            translate([0, 0, -8]) cylinder(d = ear_hole_d, h = 10);
        }
        // lo vit bat tam day
        for (b = bosses) translate([b[0], b[1], -1]) cylinder(d = m2_pilot, h = boss_h + T);
        // cua so man hinh (vat mep)
        on_face() {
            translate([win_x0 - win_clear, -0.01, win_vtop - win - win_clear]) hull() {
                translate([0, T - 0.6, 0]) cube([win + 2 * win_clear, 0.62, win + 2 * win_clear]);
                translate([-0.8, -1, -0.8]) cube([win + 2 * win_clear + 1.6, 1.01, win + 2 * win_clear + 1.6]);
            }
            for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
                translate([-mod_w / 2 + dx, T + 0.6, mod_v0 + dz]) rotate([-90, 0, 0]) cylinder(d = m16_pilot, h = mod_post_h + 1);
        }
        // hoc cam ung giua lung (day phang, thanh con 1 mm o dinh)
        translate([-touch[0] / 2 - touch_clear, touch_y - touch_clear, H - touch_wall - 3])
            cube([touch[0] + 2 * touch_clear, touch[1] + 2 * touch_clear, 3]);
        // luoi loa hong phai
        for (i = [0 : 7]) translate([W / 2 - T - 1, spk_pos[1] + 3 + i * 3.4, spk_pos[2] + 3]) hull() {
            translate([0, 0.8, 0.8]) rotate([0, 90, 0]) cylinder(d = 1.6, h = T + 3);
            translate([0, 0.8, spk[2] - 6.8]) rotate([0, 90, 0]) cylinder(d = 1.6, h = T + 3);
        }
        // mong: USB-C nam ngang + hoc dau cap, LED sac, khe cong tac
        translate([usb_x, y_rear - T - 1, usb_z]) rotate([-90, 0, 0]) slot(usb_hole[0], usb_hole[1], T + 2);
        translate([usb_x, y_rear - usb_plug_depth, usb_z]) rotate([-90, 0, 0]) slot(usb_plug[0], usb_plug[1], usb_plug_depth + 1);
        translate([d2_x, y_rear - T - 1, usb_z]) rotate([-90, 0, 0]) cylinder(d = d2_d, h = T + 2);
        translate([sw_x, y_rear - T - 1, sw_z]) rotate([-90, 0, 0]) slot(sw_slot[0], sw_slot[1], T + 2);
    }
}

module slot(l, w, h) { hull() for (s = [-1, 1]) translate([s * (l - w) / 2, 0, 0]) cylinder(d = w, h = h); }

module base() {
    difference() {
        union() {
            // tam day vua long mieng day
            intersection() {
                translate([0, 0, 0]) linear_extrude(base_t) offset(delta = -base_clear) projection(cut = true) translate([0, 0, -1]) inner_void();
                translate([-W, 0, 0]) cube([2 * W, L, base_t]);
            }
            // tru do PCB
            for (s = standoffs) translate([s[0], s[1], 0]) cylinder(d = 4.5, h = pcb_pos[2]);
        }
        for (b = bosses) {
            translate([b[0], b[1], -1]) cylinder(d = 2.4, h = base_t + 2);
            translate([b[0], b[1], -0.01]) cylinder(d1 = 4.4, d2 = 2.4, h = 1.2);
        }
        for (s = standoffs) translate([s[0], s[1], pcb_pos[2] - 6]) cylinder(d = m2_pilot, h = 7);
    }
}

ear_floor = 1.0;
module ear() {   // in dung: san tai nam tren ban in, chot huong xuong (in lat nguoc: chot len tren)
    difference() {
        union() {
            ear_shell(0);
            translate([0, 0, -ear_peg_len]) cylinder(d = ear_peg_d, h = ear_peg_len + 0.5);
        }
        translate([0, 0, ear_floor]) ear_shell(ear_wall);                              // rong ruot, chua san day
        translate([0, 0, -ear_peg_len - 1]) cylinder(d = 3.4, h = ear_peg_len + ear_floor + 2);   // luon chan LED 3 mm
    }
}
module ear_shell(inset) {   // tai meo: day elip det, dinh bo tron hoi nga ve sau
    hull() {
        scale([(ear_w / 2 - inset), (ear_d / 2 - inset), 1]) cylinder(r = 1, h = 1);
        translate([0, 1.2, ear_h - inset * 1.6]) scale([1, 0.7, 1]) sphere(r = max(1.8 - inset / 2, 0.7));
    }
}

module ghosts() {
    on_face() {
        color("DimGray", 0.9) translate([-mod_w / 2, T, mod_v0]) cube([mod_w, mod_t, mod_h]);
        color("Turquoise") translate([win_x0, T - 0.05, win_vtop - win]) cube([win, 0.1, win]);
    }
    color("RoyalBlue", 0.8) translate(bat_pos) cube(bat);
    color("Orange", 0.8) translate(spk_pos) cube(spk);
    color("SeaGreen", 0.8) translate(pcb_pos) cube(pcb);
    color("HotPink") translate([-touch[0] / 2, touch_y, H - touch_wall - touch[2]]) cube(touch);
}

// Khoi dai dien de kiem tra va cham (dung, co them khoang linh kien tren PCB)
module ghost_solids() {
    on_face() translate([-mod_w / 2, T + 0.05, mod_v0]) cube([mod_w, mod_t, mod_h]);
    translate(bat_pos) cube(bat);
    translate(spk_pos) cube(spk);
    translate(pcb_pos) cube([pcb[0], pcb[1], pcb[2] + pcb_comp]);
    translate([-touch[0] / 2, touch_y, H - touch_wall - touch[2]]) cube(touch);
}

ci = 0; which = 0;
module ghost_i(i) {
    // man hinh: bo vung quanh 4 lo vit (tru vit nam xuyen qua bo module, khong phai va cham)
    if (i == 0) on_face() difference() {
        translate([-mod_w / 2, T + 0.05, mod_v0]) cube([mod_w, mod_t, mod_h]);
        for (dx = [mod_hole_inset, mod_w - mod_hole_inset], dz = [mod_hole_inset, mod_h - mod_hole_inset])
            translate([-mod_w / 2 + dx, 0, mod_v0 + dz]) rotate([-90, 0, 0]) cylinder(d = mod_post_d + 0.4, h = 20);
    }
    if (i == 1) translate(bat_pos) cube(bat);
    if (i == 2) translate(spk_pos) cube(spk);
    if (i == 3) { translate(pcb_pos) cube(pcb);
                  translate(pcb_pos + [0, pcb_front_free, pcb[2]]) cube([pcb[0], pcb[1] - pcb_front_free, pcb_comp]); }
    if (i == 4) translate([-touch[0] / 2, touch_y, H - touch_wall - touch[2]]) cube(touch);
}

module placed_ears() {
    for (s = [-1, 1]) ear_frame(s) ear();
}

if (part == "body") body();
else if (part == "base") base();
else if (part == "ear") ear();
else if (part == "check_one") {
    names = ["man hinh", "pin", "loa", "PCB+linh kien", "cam ung"];
    echo(str("Kiem tra: ", names[ci], " voi ", which == 0 ? "vo than" : "tam day"));
    intersection() { if (which == 0) body(); else base(); ghost_i(ci); }
}
else if (part == "check") {
    // phai rong: linh kien khong duoc cham vo/tam day, va khong cham nhau
    intersection() { union() { body(); base(); } ghost_solids(); }
}
else {
    color("Ivory") body();
    color("LightSteelBlue") translate([0, 0, -10]) base();
    color("White", 0.9) placed_ears();
    ghosts();
}
