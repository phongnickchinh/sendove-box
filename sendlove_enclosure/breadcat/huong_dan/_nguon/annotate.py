"""Chu thich anh chup Blender + ghep anh truoc/sau -> sendlove_enclosure/breadcat/huong_dan/"""
import os
from PIL import Image, ImageDraw, ImageFont

RAW = os.path.join(os.path.dirname(os.path.abspath(__file__)), "raw")
VAR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "var")
OUT = r"P:\coddd\sendove-box\sendlove_enclosure\breadcat\huong_dan"
os.makedirs(OUT, exist_ok=True)
RED = (235, 64, 52); YEL = (255, 214, 10); WHITE = (255, 255, 255); DARK = (30, 30, 34)

def F(sz, bold=False):
    return ImageFont.truetype("arialbd.ttf" if bold else "arial.ttf", sz)

def raw(n): return Image.open(os.path.join(RAW, n + ".png")).convert("RGB")

def crop(im, box, scale=1.0):
    c = im.crop(box)
    if scale != 1.0: c = c.resize((int(c.width * scale), int(c.height * scale)), Image.LANCZOS)
    return c

def rect(d, box, col=RED, w=4, s=1.0, off=(0, 0)):
    x0, y0, x1, y1 = [(v - o) * s for v, o in zip(box, (off[0], off[1], off[0], off[1]))]
    d.rounded_rectangle((x0, y0, x1, y1), radius=6, outline=col, width=w)

def badge(d, xy, text, r=18, col=RED, s=1.0, off=(0, 0)):
    x, y = (xy[0] - off[0]) * s, (xy[1] - off[1]) * s
    d.ellipse((x - r, y - r, x + r, y + r), fill=col, outline=WHITE, width=3)
    f = F(int(r * 1.15), True)
    tw = d.textlength(text, font=f)
    d.text((x - tw / 2, y - r * 0.68), text, fill=WHITE, font=f)

def label(d, xy, text, sz=24, fg=DARK, bg=WHITE, pad=8, anchor="lt"):
    f = F(sz, True)
    lines = text.split("\n")
    w = max(d.textlength(l, font=f) for l in lines) + 2 * pad
    h = len(lines) * (sz + 6) + 2 * pad - 6
    x, y = xy
    if anchor[0] == "r": x -= w
    if anchor[0] == "m": x -= w / 2
    if anchor[1] == "b": y -= h
    if anchor[1] == "m": y -= h / 2
    d.rounded_rectangle((x, y, x + w, y + h), radius=8, fill=bg, outline=RED, width=3)
    for i, l in enumerate(lines):
        d.text((x + pad, y + pad + i * (sz + 6)), l, fill=fg, font=f)
    return (x, y, x + w, y + h)

def arrow(d, p0, p1, col=RED, w=5):
    import math
    d.line((p0, p1), fill=col, width=w)
    a = math.atan2(p1[1] - p0[1], p1[0] - p0[0]); L = 18
    for da in (2.6, -2.6):
        d.line((p1, (p1[0] + L * math.cos(a + da), p1[1] + L * math.sin(a + da))), fill=col, width=w)

def title_bar(im, text, sz=28):
    f = F(sz, True)
    out = Image.new("RGB", (im.width, im.height + sz + 24), WHITE)
    d = ImageDraw.Draw(out); d.text((12, 10), text, fill=DARK, font=f)
    out.paste(im, (0, sz + 24)); return out

def side_by_side(ims, captions, gap=16, sz=26, h=None):
    """ghep ngang; moi cot rong bang max(anh, chu thich) de chu thich khong bi cat"""
    if h: ims = [i.resize((int(i.width * h / i.height), h), Image.LANCZOS) for i in ims]
    f = F(sz, True); tmp = ImageDraw.Draw(Image.new("RGB", (1, 1)))
    nl = max(c.count("\n") + 1 for c in captions); top = nl * (sz + 6) + 20
    cols = [max(im.width, max(tmp.textlength(l, font=f) for l in c.split("\n")) + 12) for im, c in zip(ims, captions)]
    W = int(sum(cols) + gap * (len(ims) - 1)); Hh = max(i.height for i in ims) + top
    out = Image.new("RGB", (W, Hh), WHITE); d = ImageDraw.Draw(out); x = 0
    for im, cap, cw in zip(ims, captions, cols):
        for i, l in enumerate(cap.split("\n")):
            d.text((x + 6, 8 + i * (sz + 6)), l, fill=DARK, font=f)
        out.paste(im, (int(x), top)); x += cw + gap
    return out

def save(im, name):
    im.save(os.path.join(OUT, name)); print("saved", name, im.size)

VIEW = (2, 26, 1313, 813)

# ---------------------------------------------------------------- H01 file diagram
def h01():
    W, Hh = 1400, 660
    im = Image.new("RGB", (W, Hh), (248, 247, 244)); d = ImageDraw.Draw(im)
    def box(x, y, w, h, t1, t2, col):
        d.rounded_rectangle((x, y, x + w, y + h), radius=14, fill=col, outline=(90, 90, 90), width=2)
        d.text((x + 16, y + 14), t1, fill=DARK, font=F(26, True))
        for i, l in enumerate(t2.split("\n")):
            d.text((x + 16, y + 52 + i * 30), l, fill=(60, 60, 60), font=F(22))
    box(30, 190, 330, 170, "breadcat.py", "Thông số dáng (rộng, tai,\nđuôi, trái tim...). Đây là\n'bản gốc' của thiết kế.", (255, 236, 200))
    box(470, 210, 300, 130, "chay_lai.bat", "Bấm đúp để dựng lại\n(khoảng 3 phút)", (220, 235, 255))
    arrow(d, (360, 275), (465, 275)); arrow(d, (770, 275), (870, 275))
    outs = [("vo_than.stl, tam_day.stl,\ntai_phai.stl, tai_trai.stl", "đem đi in"),
            ("breadcat_vo.blend", "mở bằng Blender để XEM"),
            ("kiem_tra.txt", "bảng kiểm tra va chạm + kích thước thật"),
            ("pcb_outline.dxf + ảnh *.png", "đường bao PCB cho KiCad, ảnh xem trước")]
    y = 30
    for t1, t2 in outs:
        hh = 110 if "\n" in t1 else 88
        d.rounded_rectangle((880, y, 1370, y + hh), radius=12, fill=(225, 245, 225), outline=(90, 90, 90), width=2)
        d.text((896, y + 10), t1, fill=DARK, font=F(24, True))
        d.text((896, y + hh - 34), t2, fill=(60, 60, 60), font=F(20))
        arrow(d, (870, 275), (878, y + hh / 2), w=3)
        y += hh + 20
    d.text((30, 540), "Sửa trong Blender (kéo đỉnh, chỉnh tay) KHÔNG tự chảy về breadcat.py hay file STL.", fill=RED, font=F(24, True))
    d.text((30, 578), "Bấm chay_lai.bat sẽ GHI ĐÈ breadcat_vo.blend và 4 file STL — muốn giữ bản sửa tay thì", fill=RED, font=F(24, True))
    d.text((30, 612), "File > Save As sang tên khác trước.", fill=RED, font=F(24, True))
    save(im, "H01_so_do_file.png")

# ---------------------------------------------------------------- H02 interface
def h02():
    im = raw("01_giao_dien"); d = ImageDraw.Draw(im)
    rect(d, VIEW, YEL, 5); badge(d, (60, 790), "1", 24)
    rect(d, (1316, 26, 1598, 182)); badge(d, (1300, 60), "2", 24)
    rect(d, (1316, 186, 1598, 877)); badge(d, (1300, 520), "3", 24)
    rect(d, (2, 816, 1313, 877)); badge(d, (1290, 846), "4", 24)
    rect(d, (1218, 82, 1308, 296)); badge(d, (1190, 180), "5", 24)
    rect(d, (1180, 26, 1306, 52)); badge(d, (1160, 70), "6", 24)
    rect(d, (6, 84, 54, 462)); badge(d, (80, 250), "7", 24)
    rect(d, (262, 2, 318, 24)); rect(d, (1040, 2, 1100, 24)); badge(d, (680, 58), "8", 24)
    arrow(d, (660, 58), (330, 18), w=3); arrow(d, (700, 58), (1030, 18), w=3)
    save(im, "H02_giao_dien.png")

# ---------------------------------------------------------------- H03 gizmo
def h03():
    s = 2.6; off = (1150, 25)
    c = crop(raw("01_giao_dien"), (1150, 25, 1320, 300), s)
    W = c.width + 760
    im = Image.new("RGB", (W, c.height), (44, 44, 48)); im.paste(c, (0, 0)); d = ImageDraw.Draw(im)
    items = [((1261, 128), "KÉO quả cầu trục = XOAY mô hình\nBẤM chữ X / Y / Z = nhìn thẳng từ phía đó", 120),
             ((1290, 187), "KÉO kính lúp lên/xuống = PHÓNG TO / THU NHỎ", 300),
             ((1290, 218), "KÉO bàn tay = KÉO DỊCH khung nhìn", 390),
             ((1290, 248), "Máy quay = nhìn qua camera (không cần)", 480),
             ((1290, 278), "Lưới = đổi phối cảnh ↔ song song", 570)]
    for (x, y), t, ly in items:
        px, py = (x - off[0]) * s, (y - off[1]) * s
        b = label(d, (c.width + 30, ly), t, sz=26, anchor="lm")
        arrow(d, (b[0], ly), (px + 26, py), YEL, 4)
    save(im, "H03_gizmo.png")

# ---------------------------------------------------------------- H04 mouse
def h04():
    W, Hh = 1400, 560
    im = Image.new("RGB", (W, Hh), (248, 247, 244)); d = ImageDraw.Draw(im)
    def mouse(cx, cy, hl):
        d.rounded_rectangle((cx - 90, cy - 150, cx + 90, cy + 150), radius=90, fill=WHITE, outline=DARK, width=4)
        d.line((cx - 90, cy - 40, cx + 90, cy - 40), fill=DARK, width=3); d.line((cx, cy - 150, cx, cy - 40), fill=DARK, width=3)
        d.rounded_rectangle((cx - 14, cy - 120, cx + 14, cy - 60), radius=12, fill=YEL if hl else (210, 210, 210), outline=DARK, width=3)
    specs = [(230, "GIỮ con lăn + kéo chuột", "= XOAY", "⟲"), (700, "Shift + GIỮ con lăn + kéo", "= KÉO DỊCH", "✥"),
             (1170, "LĂN con lăn", "= PHÓNG TO / THU NHỎ", "±")]
    for cx, t1, t2, sym in specs:
        mouse(cx, 250, True)
        d.text((cx - d.textlength(t1, font=F(26, True)) / 2, 440), t1, fill=DARK, font=F(26, True))
        d.text((cx - d.textlength(t2, font=F(28, True)) / 2, 480), t2, fill=RED, font=F(28, True))
    d.text((30, 20), "Chuột có con lăn (nút giữa):", fill=DARK, font=F(28, True))
    arrow(d, (160, 150), (120, 120), RED, 5); arrow(d, (300, 150), (340, 120), RED, 5)
    arrow(d, (630, 130), (590, 130), RED, 5); arrow(d, (770, 130), (810, 130), RED, 5)
    arrow(d, (1170, 110), (1170, 70), RED, 5); arrow(d, (1170, 140), (1170, 180), RED, 5)
    save(im, "H04_chuot.png")

# ---------------------------------------------------------------- H05 preferences
def h05():
    im = crop(raw("09_prefs_input"), (0, 0, 1310, 430)); d = ImageDraw.Draw(im)
    rect(d, (62, 2, 92, 24)); badge(d, (110, 14), "1", 16)
    rect(d, (10, 353, 154, 377)); badge(d, (172, 365), "2", 16)
    rect(d, (446, 172, 600, 192)); badge(d, (430, 182), "3", 16)
    rect(d, (660, 87, 780, 107)); badge(d, (644, 97), "4", 16)
    rect(d, (1186, 29, 1306, 48)); badge(d, (1170, 38), "5", 16)
    save(im, "H05_prefs_input.png")

# ---------------------------------------------------------------- H06 lost view -> Home
def h06():
    a = crop(raw("02_lac"), VIEW); b = crop(raw("03_frame_all"), VIEW)
    save(side_by_side([a, b], ["Lạc góc nhìn: chỉ thấy một mảng", "Bấm phím Home → thấy lại cả mô hình"], h=430), "H06_lac_home.png")

# ---------------------------------------------------------------- H07 menu View
def h07():
    s = 1.5; off = (0, 26)
    im = crop(raw("04_menu_view"), (0, 26, 700, 500), s); d = ImageDraw.Draw(im)
    for box in ((168, 158, 422, 178), (168, 178, 422, 198), (168, 198, 422, 218), (168, 292, 422, 312), (168, 71, 422, 91)):
        rect(d, box, s=s, off=off)
    label(d, (650, 170), "Frame Selected (phím . số) =\nphóng vừa vật đang chọn", 24, anchor="lm")
    label(d, (650, 280), "Frame All (Home) = cả mô hình", 24, anchor="lm")
    label(d, (650, 360), "Perspective/Orthographic (5)\n= phối cảnh ↔ song song", 24, anchor="lm")
    label(d, (650, 470), "Viewpoint = nhìn Trước/Sau/\nTrái/Phải/Trên/Dưới", 24, anchor="lm")
    label(d, (650, 70), "Sidebar (N) = bảng số đo", 24, anchor="lm")
    save(im, "H07_menu_view.png")

# ---------------------------------------------------------------- H08 standard views
def h08():
    ims = [crop(raw(n), (200, 60, 1113, 813)) for n in ("05_front", "05_right", "05_top")]
    save(side_by_side(ims, ["Phím 1: nhìn trước", "Phím 3: nhìn bên phải", "Phím 7: nhìn từ trên"], h=420), "H08_goc_chuan.png")

# ---------------------------------------------------------------- H09 hide / show
def h09():
    s = 2.2; off = (1316, 26)
    o = crop(raw("06_an_than"), (1316, 26, 1598, 182), s); d = ImageDraw.Draw(o)
    rect(d, (1550, 97, 1566, 114), s=s, off=off)
    v = crop(raw("06_an_than"), (300, 150, 1100, 700)); dv = ImageDraw.Draw(v)
    for (x, y), t in (((450, 330), "màn hình"), ((250, 300), "PCB"), ((335, 430), "pin"), ((460, 165), "loa"),
                      ((325, 95), "cảm ứng"), ((370, 150), "tai"), ((260, 490), "tấm đáy")):
        label(dv, (x + 40, y - 20), t, 22)
        dv.ellipse((x - 6, y - 6, x + 6, y + 6), fill=YEL, outline=DARK)
    save(side_by_side([o, v], ["Outliner: bấm con mắt để ẩn/hiện", "Ẩn vo_than → thấy linh kiện bên trong"], h=500), "H09_an_hien.png")

# ---------------------------------------------------------------- H10 xray
def h10():
    s = 3.0; off = (1150, 26)
    hdr = crop(raw("07_xray"), (1150, 26, 1313, 52), s); d = ImageDraw.Draw(hdr)
    rect(d, (1180, 27, 1204, 50), s=s, off=off); rect(d, (1206, 27, 1286, 50), YEL, s=s, off=off)
    v = crop(raw("07_xray"), (300, 150, 1100, 700))
    hdr2 = Image.new("RGB", (hdr.width, v.height), (44, 44, 48)); hdr2.paste(hdr, (0, 0))
    dd = ImageDraw.Draw(hdr2)
    dd.text((10, hdr.height + 20), "Đỏ: X-ray (Alt+Z)", fill=WHITE, font=F(24, True))
    dd.text((10, hdr.height + 56), "Vàng: kiểu hiển thị", fill=WHITE, font=F(24, True))
    dd.text((10, hdr.height + 92), "(khung dây / đặc / vật liệu / render)", fill=WHITE, font=F(20))
    save(side_by_side([hdr2, v], ["Nút trên thanh tiêu đề", "Bật X-ray: vỏ trong suốt"], h=460), "H10_xray.png")

# ---------------------------------------------------------------- H11 N panel
def h11():
    s = 1.5; off = (740, 26)
    im = crop(raw("08_bang_n"), (740, 26, 1313, 520), s); d = ImageDraw.Draw(im)
    rect(d, (1044, 380, 1278, 470), s=s, off=off); rect(d, (1290, 82, 1312, 122), s=s, off=off)
    label(d, (20, 420), "vo_than được chọn\n(viền cam)", 24)
    label(d, (20, 600), "Dimensions = kích thước\nngoài của vật (mm)", 24)
    save(im, "H11_bang_n.png")

# ---------------------------------------------------------------- H12 scripting
def h12():
    im = crop(raw("10_scripting"), (0, 0, 1313, 470)); d = ImageDraw.Draw(im)
    rect(d, (1040, 2, 1100, 24)); badge(d, (1120, 13), "1", 16)
    rect(d, (560, 30, 598, 50)); badge(d, (612, 40), "2", 16)
    rect(d, (505, 86, 1140, 212), YEL); badge(d, (1160, 150), "3", 16)
    save(im, "H12_scripting.png")
    im = crop(raw("10b_tail_params"), (469, 26, 1313, 300)); d = ImageDraw.Draw(im)
    rect(d, (36, 128, 700, 168), YEL)
    save(im, "H12b_duoi_tim.png")
    s = 1.4; off = (469, 26)
    im = crop(raw("10c_menu_text"), (469, 26, 900, 400), s); d = ImageDraw.Draw(im)
    rect(d, (560, 72, 745, 92), s=s, off=off); rect(d, (560, 146, 745, 166), s=s, off=off)
    rect(d, (560, 267, 745, 287), (120, 120, 120), s=s, off=off)
    d.line(((560 - off[0]) * s, (277 - off[1]) * s, (745 - off[0]) * s, (277 - off[1]) * s), fill=RED, width=4)
    save(im, "H12c_menu_text.png")

# ---------------------------------------------------------------- before / after
def pair(k, view, cap_a, cap_b, name):
    a = Image.open(os.path.join(VAR, "goc", f"shape_{view}.png")).convert("RGB")
    b = Image.open(os.path.join(VAR, k, f"shape_{view}.png")).convert("RGB")
    save(side_by_side([a, b], [cap_a, cap_b], h=420), name)

def pairs():
    pair("ear60", "front", "Hiện tại: Z_EAR = 64.0", "Đổi: Z_EAR = 57.0 (tai thấp)", "P1_Z_EAR.png")
    pair("notch49", "front", "Hiện tại: Z_NOTCH = 45.5", "Đổi: Z_NOTCH = 49.0 (mép giữa 2 tai cao lên)", "P2_Z_NOTCH.png")
    pair("dome57", "side", "Hiện tại: Z_DOME = 52.5", "Đổi: Z_DOME = 57.0 (lưng vồng cao)", "P3_Z_DOME.png")
    pair("heart16", "rear", "Hiện tại: HEART_W = 22.0", "Đổi: HEART_W = 16.0 (tim nhỏ)", "P4_HEART_W.png")
    pair("tailz35", "side", "Hiện tại: TAIL z=41.0", "Đổi: TAIL z=35.0 (đuôi mọc thấp)", "P5_TAIL_z.png")
    pair("taper0", "front", "Hiện tại: W = 45.35, TAPER = 3.45", "Đổi: W = 48.0, TAPER = 0.0 (thân thẳng)", "P6_W_TAPER.png")
    pair("L72", "side", "Hiện tại: L = 62.0", "Đổi: L = 72.0 (thân dài)", "P7_L.png")

# ---------------------------------------------------------------- engraving text
def h13():
    # menu File > Export > STL, Save As
    s = 1.3
    im = crop(raw("15_menu_file_export"), (0, 0, 760, 470), s); d = ImageDraw.Draw(im)
    for box in ((28, 153, 225, 173), (28, 307, 225, 327), (262, 426, 500, 446)):
        rect(d, box, s=s)
    save(im, "H13_menu_file.png")
    # add text + the text properties panel
    v = crop(raw("11_chu_them"), (2, 26, 1313, 813)); dv = ImageDraw.Draw(v)
    rect(dv, (258, 2, 286, 22), off=(0, 0))
    label(dv, (300, 40), "Add > Text (Shift+A)", 26)
    p = crop(raw("11b_text_props"), (1310, 330, 1600, 880)); dp = ImageDraw.Draw(p); off = (1310, 330)
    rect(dp, (1318, 518, 1342, 540), off=off); rect(dp, (1440, 392, 1562, 414), off=off)
    rect(dp, (1440, 774, 1562, 796), off=off)
    save(side_by_side([v, p], ["Chữ vừa thêm (nhìn từ dưới: Ctrl+7)", "Tab chữ 'a': Extrude = độ sâu,\nSize = cỡ chữ"], h=550), "H14_them_chu.png")
    # menu convert
    s = 1.4; off = (280, 400)
    im = crop(raw("12_menu_convert"), (280, 400, 820, 760), s); d = ImageDraw.Draw(im)
    rect(d, (288, 627, 486, 647), s=s, off=off); rect(d, (488, 626, 686, 646), s=s, off=off)
    save(im, "H15_menu_convert.png")
    # boolean
    v = crop(raw("13_boolean"), (150, 120, 1150, 760))
    p = crop(raw("13_boolean"), (1310, 180, 1600, 560), 1.5); dp = ImageDraw.Draw(p); off = (1310, 180); s = 1.5
    for box in ((1320, 434, 1343, 456), (1318, 255, 1590, 275), (1498, 318, 1590, 338), (1398, 368, 1590, 388)):
        rect(dp, box, s=s, off=off)
    save(side_by_side([v, p], ["Tấm đáy đã khắc chữ (chữ đã ẩn)", "Modifier Boolean > Difference\n> Object = chữ"], h=500), "H16_boolean.png")
    # export STL
    im = raw("14_xuat_stl"); d = ImageDraw.Draw(im)
    rect(d, (924, 150, 1010, 168)); rect(d, (924, 194, 1000, 212), YEL); rect(d, (790, 568, 916, 590)); rect(d, (4, 568, 520, 590))
    label(d, (560, 250), "Selection Only: ĐÁNH DẤU\n(chỉ xuất vật đang chọn)", 22)
    label(d, (560, 330), "Scene Unit: ĐỂ TRỐNG,\nScale = 1.000 (giữ đơn vị mm)", 22)
    label(d, (200, 500), "Tên file mới, vd. tam_day_khac_chu.stl", 22)
    save(im, "H17_xuat_stl.png")

for fn in (h01, h02, h03, h04, h05, h06, h07, h08, h09, h10, h11, h12, pairs, h13):
    fn()
