"""Chay Blender co giao dien (--enable-event-simulate), tu dat tung trang thai va chup man hinh de lam tai lieu."""
import bpy, os, json, math, traceback
from mathutils import Vector

OUT = r"C:\Users\phamp\AppData\Local\Temp\claude\P--coddd-sendove-box\d65a1008-4d01-472f-bf27-49b5fb37ca9e\scratchpad\guide_bl\raw"
PY = r"P:\coddd\sendove-box\sendlove_enclosure\breadcat\breadcat.py"
os.makedirs(OUT, exist_ok=True)
META = {}
H = 900     # window height (event coordinates count from the bottom up)

def win():
    return bpy.context.window_manager.windows[0]

def area(t, w=None):
    w = w or win()
    return next((a for a in w.screen.areas if a.type == t), None)

def region(a, t='WINDOW'):
    return next(r for r in a.regions if r.type == t)

def v3d():
    a = area('VIEW_3D'); return a, region(a), a.spaces.active

def shot(name, w=None):
    w = w or win()
    with bpy.context.temp_override(window=w):
        bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
        bpy.ops.screen.screenshot(filepath=os.path.join(OUT, name + ".png"), check_existing=False)
    META[name] = dict(win_h=w.height, win_w=w.width,
                      areas=[dict(type=a.type, x=a.x, y=a.y, w=a.width, h=a.height) for a in w.screen.areas])
    print("SHOT", name)

def move(x, y_img, w=None):
    """toa do theo anh chup (goc tren-trai)"""
    (w or win()).event_simulate(type='MOUSEMOVE', value='NOTHING', x=x, y=H - y_img)

def click(x, y_img, w=None):
    w = w or win(); move(x, y_img, w)
    w.event_simulate(type='LEFTMOUSE', value='PRESS', x=x, y=H - y_img)
    w.event_simulate(type='LEFTMOUSE', value='RELEASE', x=x, y=H - y_img)

def esc(n=1, w=None):
    w = w or win()
    for _ in range(n):
        w.event_simulate(type='ESC', value='PRESS'); w.event_simulate(type='ESC', value='RELEASE')

def op3d(fn, **kw):
    a, r, s = v3d()
    with bpy.context.temp_override(window=win(), area=a, region=r, space_data=s):
        return fn(**kw)

def set_view(loc, eye, dist=None, persp='PERSP'):
    a, r, s = v3d(); r3 = s.region_3d
    loc = Vector(loc); eye = Vector(eye)
    r3.view_location = loc; r3.view_rotation = (loc - eye).to_track_quat('-Z', 'Y')
    r3.view_distance = dist if dist else (loc - eye).length; r3.view_perspective = persp

def only_visible(names):
    for o in bpy.context.scene.objects:
        if o.name.startswith("Breadcat_cage"): continue
        o.hide_set(o.name not in names)

def select(names, active=None):
    for o in bpy.context.scene.objects: o.select_set(False)
    for n in names: bpy.data.objects[n].select_set(True)
    bpy.context.view_layer.objects.active = bpy.data.objects[active] if active else None

def steps():
    select(["vo_than"], "vo_than")
    move(700, 500)
    yield 2.0
    shot("01_giao_dien")
    # lost view (as in the old file) -> Frame All
    set_view((0, 0, 0), (-6, -9, 6), dist=12)
    yield 0.6
    shot("02_lac")
    op3d(bpy.ops.view3d.view_all, center=False)
    yield 1.2
    shot("03_frame_all")
    # menu View
    click(184, 38); yield 1.0
    shot("04_menu_view")
    esc(); yield 0.6
    # standard views
    for nm, t in (("05_front", 'FRONT'), ("05_right", 'RIGHT'), ("05_top", 'TOP')):
        op3d(bpy.ops.view3d.view_axis, type=t)
        op3d(bpy.ops.view3d.view_all, center=False)
        yield 1.0
        shot(nm)
    set_view((0, 31, 30), (-150, -230, 150), dist=190)
    yield 0.8
    # hide the shell body -> components inside become visible
    bpy.data.objects["vo_than"].hide_set(True)
    select([], None)
    set_view((0, 31, 22), (-120, -170, 120), dist=150)
    yield 0.8
    shot("06_an_than")
    bpy.data.objects["vo_than"].hide_set(False)
    set_view((0, 31, 30), (-150, -230, 150), dist=190)
    # X-ray
    a, r, s = v3d()
    s.shading.show_xray = True; s.shading.xray_alpha = 0.35
    yield 0.8
    shot("07_xray")
    s.shading.show_xray = False
    # N panel - dimensions
    select(["vo_than"], "vo_than")
    s.show_region_ui = True
    yield 1.0
    shot("08_bang_n")
    s.show_region_ui = False
    # Preferences > Input
    a = area('VIEW_3D')
    bpy.context.preferences.active_section = 'INPUT'
    a.ui_type = 'PREFERENCES'
    yield 1.2
    shot("09_prefs_input")
    a.ui_type = 'VIEW_3D'
    yield 0.8
    # menu File (Save As, Export)
    click(40, 13); yield 0.8
    move(73, 317); yield 1.2
    shot("15_menu_file_export")
    esc(2); yield 0.6
    # Scripting: open breadcat.py
    w = win()
    w.workspace = bpy.data.workspaces['Scripting']
    yield 1.5
    te = area('TEXT_EDITOR')
    txt = bpy.data.texts.load(PY)
    sp = te.spaces.active; sp.text = txt; sp.show_line_numbers = True; sp.show_syntax_highlight = True
    sp.top = 10
    yield 1.2
    shot("10_scripting")
    w.workspace = bpy.data.workspaces['Layout']
    yield 1.5
    # ----- example: engraving text under the base plate
    only_visible({"tam_day"})
    a, r, s = v3d()
    with bpy.context.temp_override(window=win(), area=a, region=r, space_data=s):
        bpy.ops.object.text_add(location=(0, 34, 0), rotation=(math.pi, 0, 0))
    t = bpy.context.view_layer.objects.active
    t.name = "chu_khac"
    t.data.body = "SendLove"; t.data.size = 7.0; t.data.extrude = 0.6
    t.data.align_x = 'CENTER'; t.data.align_y = 'CENTER'
    t.color = (0.85, 0.2, 0.3, 1)
    select(["chu_khac"], "chu_khac")
    op3d(bpy.ops.view3d.view_axis, type='BOTTOM')
    op3d(bpy.ops.view3d.view_all, center=False)
    pa = area('PROPERTIES'); pa.spaces.active.context = 'DATA'
    move(700, 500)
    yield 1.2
    shot("11_chu_them")
    # menu Object > Convert
    click(312, 38); yield 0.8
    move(342, 637); yield 1.2
    shot("12_menu_convert")
    esc(2); yield 0.6
    op3d(bpy.ops.object.convert, target='MESH')
    yield 0.5
    plate = bpy.data.objects["tam_day"]
    m = plate.modifiers.new("Boolean", 'BOOLEAN'); m.operation = 'DIFFERENCE'; m.object = bpy.data.objects["chu_khac"]
    bpy.data.objects["chu_khac"].hide_set(True)
    select(["tam_day"], "tam_day")
    pa.spaces.active.context = 'MODIFIER'
    a, r, s = v3d()
    s.shading.show_cavity = True
    set_view((0, 31, 0), (-40, -60, -110), dist=95)
    yield 1.5
    shot("13_boolean")
    # STL export dialog
    a, r, s = v3d()
    with bpy.context.temp_override(window=win(), area=a, region=r):
        bpy.ops.wm.stl_export('INVOKE_DEFAULT')
    yield 2.0
    ws = bpy.context.window_manager.windows
    shot("14_xuat_stl", ws[-1])
    esc(1, ws[-1])
    yield 0.8

GEN = steps()

def tick():
    try:
        return next(GEN)
    except StopIteration:
        json.dump(META, open(os.path.join(OUT, "meta.json"), "w"), indent=1)
        print("ALL_DONE")
        bpy.ops.wm.quit_blender()
        return None
    except Exception:
        traceback.print_exc()
        json.dump(META, open(os.path.join(OUT, "meta.json"), "w"), indent=1)
        bpy.ops.wm.quit_blender()
        return None

bpy.app.timers.register(tick, first_interval=3.0)
