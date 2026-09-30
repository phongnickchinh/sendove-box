"""Chup them: doan TAIL/HEART_W trong script, menu Text, bang Geometry/Font cua chu khac."""
import bpy, os, math, traceback, json

OUT = r"C:\Users\phamp\AppData\Local\Temp\claude\P--coddd-sendove-box\d65a1008-4d01-472f-bf27-49b5fb37ca9e\scratchpad\guide_bl\raw"
PY = r"P:\coddd\sendove-box\sendlove_enclosure\breadcat\breadcat.py"
H = 900

def win(): return bpy.context.window_manager.windows[0]
def area(t): return next((a for a in win().screen.areas if a.type == t), None)
def shot(name):
    with bpy.context.temp_override(window=win()):
        bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
        bpy.ops.screen.screenshot(filepath=os.path.join(OUT, name + ".png"), check_existing=False)
    print("SHOT", name)
def move(x, y): win().event_simulate(type='MOUSEMOVE', value='NOTHING', x=x, y=H - y)
def click(x, y):
    move(x, y)
    win().event_simulate(type='LEFTMOUSE', value='PRESS', x=x, y=H - y)
    win().event_simulate(type='LEFTMOUSE', value='RELEASE', x=x, y=H - y)
def esc(n=1):
    for _ in range(n):
        win().event_simulate(type='ESC', value='PRESS'); win().event_simulate(type='ESC', value='RELEASE')

def steps():
    yield 2.0
    w = win(); w.workspace = bpy.data.workspaces['Scripting']
    yield 1.5
    te = area('TEXT_EDITOR'); sp = te.spaces.active
    txt = bpy.data.texts.load(PY); sp.text = txt; sp.show_line_numbers = True; sp.show_syntax_highlight = True
    sp.top = 118
    yield 1.0
    shot("10b_tail_params")
    click(573, 39); yield 1.0
    shot("10c_menu_text")
    esc(); yield 0.6
    w.workspace = bpy.data.workspaces['Layout']
    yield 1.5
    for o in bpy.context.scene.objects:
        if not o.name.startswith("Breadcat_cage"): o.hide_set(o.name != "tam_day")
    a = area('VIEW_3D'); r = next(r for r in a.regions if r.type == 'WINDOW')
    with bpy.context.temp_override(window=win(), area=a, region=r, space_data=a.spaces.active):
        bpy.ops.object.text_add(location=(0, 34, 0), rotation=(math.pi, 0, 0))
        bpy.ops.view3d.view_axis(type='BOTTOM')
    t = bpy.context.view_layer.objects.active
    t.data.body = "SendLove"; t.data.size = 7.0; t.data.extrude = 0.6
    t.data.align_x = 'CENTER'; t.data.align_y = 'CENTER'
    pa = area('PROPERTIES'); pa.spaces.active.context = 'DATA'
    move(700, 500)
    yield 1.2
    click(1400, 298); yield 0.6     # dong Shape
    click(1400, 406); yield 0.6     # dong Paragraph
    click(1400, 378); yield 0.6     # mo Font
    click(1400, 351); yield 0.8     # mo Geometry
    move(700, 500); yield 0.5
    shot("11b_text_props")
    yield 0.3

G = steps()
def tick():
    try: return next(G)
    except StopIteration:
        bpy.ops.wm.quit_blender(); return None
    except Exception:
        traceback.print_exc(); bpy.ops.wm.quit_blender(); return None
bpy.app.timers.register(tick, first_interval=3.0)
