# Vo SendLove dang "Breadcat" (meo o banh mi) - dung trong Blender 5.2
# Chay: blender -b --factory-startup --python breadcat.py -- <stage> <outdir>
#   stage = shape : dung dang ngoai + render de duyet
# Toa do: x ngang, y truoc->sau (mat meo o y=0, huong -y), z len. Don vi mm.
import bpy, bmesh, math, sys, os
from mathutils import Vector, Matrix

ARGV = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
STAGE = ARGV[0] if ARGV else "shape"
OUT = os.path.abspath(ARGV[1]) if len(ARGV) > 1 else os.path.dirname(os.path.abspath(__file__))

# ---------------------------------------------------------------- tham so dang
W = 45.35         # rong than o vai (mep tren thanh)
L = 62.0          # dai
HW = W / 2
Z_SHOULDER = 44.5 # vai: noi thanh ben chuyen sang vom
Z_NOTCH = 45.5    # mep tren giua 2 tai (phia truoc)
Z_DOME = 52.5     # dinh vom phia sau tai
Z_EAR = 64.0      # dinh tai (luoi dieu khien, be mat that thap hon ~2)
EAR_IN = 7.0      # mep trong chan tai (x)
TAPER = 3.45      # than hinh thang; W, TAPER chinh de do thuc: day 51, dinh thanh 46 mm

def wx(z):
    """nua be rong thanh than o do cao z (hinh thang, rong dan xuong day)"""
    return HW + TAPER * (Z_SHOULDER - z) / Z_SHOULDER

def half_profile(ear, top, deep=False):
    """nua mat cat ngang (x>=0) tu day giua -> dinh giua. ear: 0..1 do cao tai, top: z dinh vom tai mat cat nay.
    deep: keo day xuong -14 (thanh dung thang) de lam khoang rong mo day"""
    zb = -14.0 if deep else -1.0
    base = [(0.0, zb), (10.0, zb), (wx(zb) - 2.5, zb), (wx(zb + 3.0), zb + 3.0), (wx(14.0), 14.0), (wx(25.0), 25.0),
            (wx(31.0), 31.0), (wx(38.0), 38.0), (wx(Z_SHOULDER) - 1.0, Z_SHOULDER)]
    dome = [(HW - 4.5, top - 3.0), (HW - 8.5, top - 1.2), (EAR_IN, top - 0.4), (4.5, top - 0.1), (0.0, top)]
    earp = [(HW - 2.4, 50.5), (HW - 5.0, Z_EAR), (EAR_IN, Z_NOTCH + 1.5), (3.8, Z_NOTCH + 0.2), (0.0, Z_NOTCH)]
    upper = [(d[0] + (e[0] - d[0]) * ear, d[1] + (e[1] - d[1]) * ear) for d, e in zip(dome, earp)]
    return base + upper

def full_loop(half, y, sx=1.0, zc=24.0, sz=1.0):
    pts = [(x * sx, zc + (z - zc) * sz if z > zc else z) for x, z in half]   # chi co phan tren, giu day de than khong loe
    right = pts                      # tu day giua -> dinh giua ben phai
    left = [(-x, z) for x, z in reversed(pts[1:-1])]   # dinh -> day ben trai (bo 2 diem giua trung)
    return [(x, y, z) for x, z in right + left]

# cac mat cat doc truc y: (y, do cao tai, dinh vom, he so co x, he so co z)
SECTIONS = [
    (-0.6, 1.00, Z_NOTCH, 0.90, 0.95),   # mat truoc (thu vao de bo mep)
    (1.2,  1.00, Z_NOTCH, 1.00, 1.00),
    (5.5,  1.00, Z_NOTCH, 1.00, 1.00),
    (9.5,  0.60, 49.0,    1.00, 1.00),
    (13.5, 0.20, 50.5,    1.00, 1.00),
    (18.0, 0.00, Z_DOME,  1.00, 1.00),
    (34.0, 0.00, Z_DOME,  1.00, 1.00),
    (50.0, 0.00, 50.0,    1.00, 1.00),
    (57.5, 0.00, 48.0,    1.00, 1.00),
    (61.0, 0.00, 46.0,    0.97, 0.97),
    (L + 0.6, 0.00, 45.0, 0.88, 0.93),   # mat sau
]

def build_cage(name, with_ears=True, deep=False):
    """than o banh mi + tai - luoi dieu khien + Subdivision, sua duoc trong Blender"""
    bm = bmesh.new()
    loops = []
    for (y, ear, top, sx, sz) in SECTIONS:
        h = half_profile(ear if with_ears else 0.0, top, deep)
        loops.append([bm.verts.new(p) for p in full_loop(h, y, sx, 24.0, sz)])
    n = len(loops[0])
    for a, b in zip(loops, loops[1:]):
        for j in range(n):
            bm.faces.new((a[j], a[(j + 1) % n], b[(j + 1) % n], b[j]))
    for lp in (loops[0], loops[-1]):
        y = lp[0].co.y
        inner = [bm.verts.new((v.co.x * 0.45, y, 24.0 + (v.co.z - 24.0) * 0.45)) for v in lp]
        for j in range(n):
            bm.faces.new((lp[j], lp[(j + 1) % n], inner[(j + 1) % n], inner[j]))
        bm.faces.new(inner)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    m = ob.modifiers.new("Subdivision", "SUBSURF"); m.levels = 3; m.render_levels = 3
    return ob

# ---------------------------------------------------------------- tien ich
def bake(ob, name=None):
    """tra ve object moi = mesh da ap dung modifier"""
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg))
    me.transform(ob.matrix_world)
    o2 = bpy.data.objects.new(name or ob.name + "_baked", me)
    bpy.context.scene.collection.objects.link(o2)
    return o2

def box(name, x0, x1, y0, y1, z0, z1):
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector(((x0 + x1) / 2 + v.co.x * (x1 - x0), (y0 + y1) / 2 + v.co.y * (y1 - y0), (z0 + z1) / 2 + v.co.z * (z1 - z0)))
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
    return ob

def rbox(name, size, radius, segs=3):
    """hop bo tron bang modifier Bevel, tam o goc toa do"""
    ob = box(name, -size[0] / 2, size[0] / 2, -size[1] / 2, size[1] / 2, -size[2] / 2, size[2] / 2)
    m = ob.modifiers.new("Bevel", "BEVEL"); m.width = radius; m.segments = segs; m.limit_method = 'NONE'
    return ob

def boolean(target, cutter, op='DIFFERENCE', keep=False):
    m = target.modifiers.new("Bool", "BOOLEAN"); m.operation = op; m.object = cutter; m.solver = 'MANIFOLD'
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(target.evaluated_get(dg))
    target.modifiers.remove(m)
    old = target.data; target.data = me; bpy.data.meshes.remove(old)
    if not keep: bpy.data.objects.remove(cutter)
    return target

def flatten(ob, bottom=True):
    """cat phang mat truoc / sau / day"""
    big = 200
    boolean(ob, box("cut_f", -big, big, -big, 0.0, -big, big))
    boolean(ob, box("cut_r", -big, big, L, big, -big, big))
    if bottom: boolean(ob, box("cut_b", -big, big, -big, big, -big, 0.0))
    return ob

TAIL = dict(w=10.0, t=7.0, z=41.0, out=3.0, r=6.0, up=4.0, corner=1.2)
HEART_W = 22.0   # be rong trai tim o dau duoi (mm); day = TAIL['t'], mat phang, khong bo tron

def make_tail():
    """duoi = thanh hop chu nhat di ra sau roi be cong len; dau duoi gan trai tim (make_heart)"""
    tw, tt, rc = TAIL['w'] / 2, TAIL['t'] / 2, TAIL['corner']
    sec = []                                   # mat cat chu nhat bo goc nho (u: ngang x, v: theo phap tuyen)
    for cu, cv, a0 in ((tw - rc, tt - rc, 0), (-tw + rc, tt - rc, 90), (-tw + rc, -tt + rc, 180), (tw - rc, -tt + rc, 270)):
        for i in range(4):
            a = math.radians(a0 + 30 * i)
            sec.append((cu + rc * math.cos(a), cv + rc * math.sin(a)))
    # duong tam trong mat phang y-z: doan thang -> cung 90 do -> doan thang dung
    y0, z0, R = L - 6.0, TAIL['z'], TAIL['r']
    yc = L + TAIL['out']
    path = [((y0, z0), (1, 0)), ((yc, z0), (1, 0))]            # (diem, huong di (y,z))
    for i in range(1, 9):
        a = math.radians(-90 + 90 * i / 8)
        path.append(((yc + R * math.cos(a), z0 + R + R * math.sin(a)), (-math.sin(a), math.cos(a))))
    path.append(((yc + R, z0 + R + TAIL['up']), (0, 1)))
    bm = bmesh.new(); rings = []
    for (py, pz), (ty, tz) in path:
        ny, nz = -tz, ty                       # phap tuyen trong mat phang y-z (vuong goc huong di)
        rings.append([bm.verts.new((u, py + v * ny, pz + v * nz)) for u, v in sec])
    n = len(sec)
    for a, b in zip(rings, rings[1:]):
        for j in range(n): bm.faces.new((a[j], a[(j + 1) % n], b[(j + 1) % n], b[j]))
    bm.faces.new(rings[0]); bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("tail"); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new("tail", me); bpy.context.scene.collection.objects.link(ob)
    return ob

def make_heart():
    """trai tim dau duoi: net trai tim trong mat phang x-z, ep day theo y bang do day duoi.
    Dat sao cho o do cao ngang dau thanh duoi, trai tim rong bang thanh -> mui tim giau trong thanh."""
    s = HEART_W / 32.0
    N = 160
    pts = []
    for i in range(N):
        t = 2 * math.pi * i / N
        pts.append((s * 16 * math.sin(t) ** 3,
                    s * (13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t))))
    # do cao (tinh tu tam) noi nua duoi trai tim rong bang thanh duoi + 0.6
    half = TAIL['w'] / 2 + 0.3
    zw = None
    for i in range(N // 2, 0, -1):             # di tu mui tim (t=pi) nguoc len theo nhanh phai
        if pts[i][0] >= half: zw = pts[i][1]; break
    z_end = TAIL['z'] + TAIL['r'] + TAIL['up']  # dau thanh duoi
    dz = (z_end - 1.0) - zw                     # thanh cam sau vao tim 1 mm
    yc = L + TAIL['out'] + TAIL['r']
    y0, y1 = yc - TAIL['t'] / 2, yc + TAIL['t'] / 2
    bm = bmesh.new()
    f0 = [bm.verts.new((x, y0, z + dz)) for x, z in pts]
    f1 = [bm.verts.new((x, y1, z + dz)) for x, z in pts]
    for j in range(N):
        bm.faces.new((f0[j], f0[(j + 1) % N], f1[(j + 1) % N], f1[j]))
    bm.faces.new(f0); bm.faces.new(f1)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("heart"); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new("heart", me); bpy.context.scene.collection.objects.link(ob)
    print("HEART z %.1f..%.1f" % (min(z for _, z in pts) + dz, max(z for _, z in pts) + dz))
    return ob

def hull(name, pts):
    bm = bmesh.new()
    vs = [bm.verts.new(p) for p in pts]
    bmesh.ops.convex_hull(bm, input=vs)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob); return ob

def outer_shell_shape():
    cage = build_cage("Breadcat_cage")
    outer = bake(cage, "outer")
    flatten(outer)
    boolean(outer, make_tail(), 'UNION')
    boolean(outer, make_heart(), 'UNION')
    return cage, outer

# ---------------------------------------------------------------- render
def mat_toast():
    m = bpy.data.materials.new("toast"); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; Lk = nt.links
    bsdf = N["Principled BSDF"]
    geo = N.new("ShaderNodeNewGeometry")
    sep = N.new("ShaderNodeSeparateXYZ"); Lk.new(geo.outputs["Position"], sep.inputs[0])
    nrm = N.new("ShaderNodeSeparateXYZ"); Lk.new(geo.outputs["Normal"], nrm.inputs[0])
    # do chay = theo chieu cao + theo huong mat (mat tren chay nhieu hon)
    mr = N.new("ShaderNodeMapRange"); mr.inputs[1].default_value = 22.0; mr.inputs[2].default_value = 50.0
    Lk.new(sep.outputs["Z"], mr.inputs[0])
    mr2 = N.new("ShaderNodeMapRange"); mr2.inputs[1].default_value = -0.2; mr2.inputs[2].default_value = 1.0
    Lk.new(nrm.outputs["Z"], mr2.inputs[0])
    mul = N.new("ShaderNodeMath"); mul.operation = 'MULTIPLY'
    Lk.new(mr.outputs[0], mul.inputs[0]); Lk.new(mr2.outputs[0], mul.inputs[1])
    add = N.new("ShaderNodeMath"); add.operation = 'ADD'; add.use_clamp = True
    Lk.new(mul.outputs[0], add.inputs[0]); Lk.new(mr.outputs[0], add.inputs[1])
    half = N.new("ShaderNodeMath"); half.operation = 'MULTIPLY'; half.inputs[1].default_value = 0.75
    Lk.new(add.outputs[0], half.inputs[0])
    ramp = N.new("ShaderNodeValToRGB")
    cr = ramp.color_ramp
    cr.elements[0].position = 0.0; cr.elements[0].color = (0.93, 0.86, 0.74, 1)
    cr.elements[1].position = 1.0; cr.elements[1].color = (0.36, 0.14, 0.04, 1)
    e = cr.elements.new(0.35); e.color = (0.86, 0.60, 0.30, 1)
    e = cr.elements.new(0.65); e.color = (0.66, 0.33, 0.10, 1)
    Lk.new(half.outputs[0], ramp.inputs[0])
    Lk.new(ramp.outputs[0], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = 0.65
    return m

def mat_flat(name, rgb, rough=0.6):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]; b.inputs["Base Color"].default_value = (*rgb, 1); b.inputs["Roughness"].default_value = rough
    return m

def disc(name, cx, cz, rx, rz, y=-0.05, mat=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=1.0, depth=0.1, location=(cx, y, cz), rotation=(math.radians(90), 0, 0))
    o = bpy.context.active_object; o.name = name; o.scale = (rx, rz, 1)
    if mat: o.data.materials.append(mat)
    return o

# ---- MODULE MAN HINH: 3 phan (mach PCB, khoi LCD, vung hien thi) + 4 lo vit. Don vi mm. SO DO THAT cua user (2026-10-01)
SCR = dict(
    pcb_w=27.6, pcb_h=39.0, pcb_t=1.2,       # tam mach: rong x cao x day
    lcd_w=26.0, lcd_h=29.2, lcd_t=2.0,       # khoi LCD (khung kim loai + cap): dat chinh giua mach, cach 2 dau mach (39-29.2)/2 = 4.9 (do ~4.8)
    hole_d=1.8,                              # duong kinh lo vit (do ~1.8)
    hole_edge=1.0,                           # khoang cach tu MEP LO den 2 mep PCB gan nhat (4 goc). Neu do tu TAM lo: dat = 1.0 - hole_d/2
)
MOD_Z0 = 3.0                    # mep duoi tam mach (do cao so voi mat tam day)
LCD_Z0 = MOD_Z0 + (SCR['pcb_h'] - SCR['lcd_h']) / 2      # mep duoi khoi LCD
WIN = 25.6                      # vung hien thi 25.6 x 25.6, SAT MEP TREN khoi LCD (dai FPC ~3.6 mm o phia duoi)
WIN_Z0 = LCD_Z0 + SCR['lcd_h'] - WIN                    # mep duoi vung hien thi

def face_decals():
    """mo phong hinh hien tren man hinh: nen kem + mat + ma hong"""
    panel = box("screen", -WIN / 2, WIN / 2, -0.06, 0.0, WIN_Z0, WIN_Z0 + WIN)
    panel.data.materials.append(mat_flat("screen", (0.95, 0.90, 0.82), 0.3))
    frame = box("frame", -WIN / 2 - 0.5, WIN / 2 + 0.5, -0.03, 0.0, WIN_Z0 - 0.5, WIN_Z0 + WIN + 0.5)
    frame.data.materials.append(mat_flat("gap", (0.25, 0.17, 0.12)))
    dark = mat_flat("eye", (0.12, 0.06, 0.04), 0.4); pink = mat_flat("blush", (0.97, 0.62, 0.62), 0.8)
    zc = WIN_Z0 + WIN * 0.55
    for s in (-1, 1):
        disc("eye", s * 6.2, zc, 1.5, 1.9, -0.12, dark)
        disc("blush", s * 9.0, zc - 4.2, 2.6, 1.5, -0.10, pink)
    # mieng "w"
    for s in (-1, 1):
        bpy.ops.mesh.primitive_torus_add(major_radius=1.0, minor_radius=0.22, location=(s * 1.0, -0.12, zc - 2.2), rotation=(math.radians(90), 0, 0))
        t = bpy.context.active_object; t.data.materials.append(dark)
        boolean(t, box("halfcut", -5 + s * 1.0, 5 + s * 1.0, -2, 2, zc - 2.2, zc + 3))

def setup_scene(size=(int(os.environ.get("BC_W", "1100")), int(int(os.environ.get("BC_W", "1100")) * 0.745))):
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.samples = int(os.environ.get('BC_SAMPLES', '64')); sc.cycles.use_denoising = True
    sc.render.resolution_x, sc.render.resolution_y = size
    sc.unit_settings.system = 'METRIC'; sc.unit_settings.scale_length = 0.001
    w = bpy.data.worlds.new("w"); sc.world = w; w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (0.93, 0.93, 0.95, 1)
    w.node_tree.nodes["Background"].inputs[1].default_value = 0.35
    ld = bpy.data.lights.new("key", "AREA"); ld.energy = 4.5e5; ld.size = 150
    lo = bpy.data.objects.new("key", ld); sc.collection.objects.link(lo)
    lo.location = (-120, -140, 200); look_at(lo, Vector((0, 30, 20)))
    ld2 = bpy.data.lights.new("fill", "AREA"); ld2.energy = 1.2e5; ld2.size = 200
    lo2 = bpy.data.objects.new("fill", ld2); sc.collection.objects.link(lo2)
    lo2.location = (180, -60, 90); look_at(lo2, Vector((0, 30, 20)))
    fl = box("floor", -400, 400, -400, 400, -1, 0)
    fl.data.materials.append(mat_flat("floor", (0.85, 0.80, 0.72), 0.9))
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    cam.data.lens = 85; cam.data.clip_start = 1; cam.data.clip_end = 5000
    return cam

def look_at(ob, target):
    d = target - ob.location
    ob.rotation_euler = d.to_track_quat('-Z', 'Y').to_euler()

def shot(cam, eye, target, path):
    cam.location = Vector(eye); look_at(cam, Vector(target))
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)

# ================================================================ CO KHI
T = 2.0                      # vo day
EAR_SKIN = 1.0               # lop da quanh chan tai thuoc ve tai (tao hoc dinh vi tren than)
EAR_SEAM_Z = 43.0            # duong ghep tai-than (tren dinh module 42.1)
WIN_CLR = 0.1                # o lo man hinh = WIN + 2*WIN_CLR (25.8 mm): khe hao cho in 3D / lech module
# Tam mica che man hinh nam trong ranh o mat ngoai (phang voi vo), do tren bac do bang nhua (day T - LENS['t'])
LENS = dict(w=28.6, h=28.6, t=1.0, clr=0.15)   # mica 28.6 x 28.6 x 1.0; clr = khe moi ben giua mica va ranh
PCB_Z = 12.0                # mat duoi PCB = dinh tru vit
PCB_Y = (9.5, L - T - 0.5)   # mep sau PCB cach thanh sau 0.5 (USB-C sat thanh)
BOSSES = [(sx * 18.5, yb) for sx in (-1, 1) for yb in (12.0, 56.0)]
BOSS_D, PILOT_D = 5.0, 1.7   # vit tu ren M2
BAT = (-15.0, 15.0, 9.5, 59.5, 2.2, 10.2)                  # pin 30x50x8 nam tren tam day
SPK = dict(t=7.1, l=30.0, h=20.0, y0=20.0, z0=17.3)         # loa dung sat thanh phai
TOUCH = dict(w=8.8, l=12.7, t=0.4, yc=36.0, wall=1.0, clr=0.3)
USB = dict(x=-4.0, z=PCB_Z + 1.6 + 1.6, plug=(12.5, 7.0), slot=(13.1, 7.6))   # than dau cam 12.5x7 GIA DINH
D2 = dict(x=-14.0, d=2.0)
SW = dict(x=10.0, z=PCB_Z + 1.6 + 1.8, slot=(6.0, 3.0))
LED = dict(x=17.0, y=3.5, bore=4.0, z0=41.0, z1=47.0)       # lo 4 mm cho LED 3 mm co vanh / LED han san day

def link(ob, coll):
    for c in ob.users_collection: c.objects.unlink(ob)
    coll.objects.link(ob)

def dup(ob, name):
    o = bpy.data.objects.new(name, ob.data.copy()); bpy.context.scene.collection.objects.link(o); return o

def sdf_offset(src, dist, name, voxel=0.25):
    """offset that (khong tu cat) bang luoi SDF cua Geometry Nodes. dist<0 = co vao"""
    o = dup(src, name)
    ng = bpy.data.node_groups.new(name + "_gn", "GeometryNodeTree")
    ng.interface.new_socket(name="Geometry", in_out='INPUT', socket_type='NodeSocketGeometry')
    ng.interface.new_socket(name="Geometry", in_out='OUTPUT', socket_type='NodeSocketGeometry')
    gi = ng.nodes.new("NodeGroupInput"); go = ng.nodes.new("NodeGroupOutput")
    m2s = ng.nodes.new("GeometryNodeMeshToSDFGrid"); m2s.inputs["Voxel Size"].default_value = voxel
    m2s.inputs["Band Width"].default_value = int(abs(dist) / voxel) + 4
    off = ng.nodes.new("GeometryNodeSDFGridOffset"); off.inputs["Distance"].default_value = dist
    g2m = ng.nodes.new("GeometryNodeGridToMesh")
    ng.links.new(gi.outputs[0], m2s.inputs["Mesh"]); ng.links.new(m2s.outputs[0], off.inputs["Grid"])
    ng.links.new(off.outputs[0], g2m.inputs["Grid"]); ng.links.new(g2m.outputs[0], go.inputs[0])
    md = o.modifiers.new("sdf", "NODES"); md.node_group = ng
    r = bake(o, name); bpy.data.objects.remove(o); bpy.data.node_groups.remove(ng)
    return r

def cyl(name, p0, p1, r, seg=32):
    """tru tu p0 den p1"""
    p0, p1 = Vector(p0), Vector(p1); d = p1 - p0
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r, radius2=r, depth=d.length)
    rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((p0 + p1) / 2) @ rot, verts=bm.verts)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob); return ob

def cone(name, p0, r0, p1, r1, seg=32):
    p0, p1 = Vector(p0), Vector(p1); d = p1 - p0
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=seg, radius1=r0, radius2=r1, depth=d.length)
    rot = d.to_track_quat('Z', 'Y').to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation((p0 + p1) / 2) @ rot, verts=bm.verts)
    me = bpy.data.meshes.new(name); bm.to_mesh(me); bm.free()
    ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob); return ob

def slot_y(name, x, z, w, h, y0, y1, seg=16):
    """lo oval (rong w, cao h) xuyen theo truc y"""
    r = h / 2; pts = []
    for cx, a0 in ((x - w / 2 + r, math.pi / 2), (x + w / 2 - r, -math.pi / 2)):
        for i in range(seg + 1):
            a = a0 + math.pi * i / seg
            pts += [(cx + r * math.cos(a), y, z + r * math.sin(a)) for y in (y0, y1)]
    return hull(name, pts)

def ray_hit(ob, origin, direction):
    ok, loc, n, i = ob.ray_cast(Vector(origin), Vector(direction).normalized())
    return loc if ok else None

def mesh_stats(ob):
    bm = bmesh.new(); bm.from_mesh(ob.data)
    nm = sum(1 for e in bm.edges if not e.is_manifold)
    # dem manh roi
    seen = set(); parts = 0
    for v in bm.verts:
        if v.index in seen: continue
        parts += 1; st = [v]; seen.add(v.index)
        while st:
            a = st.pop()
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in seen: seen.add(b.index); st.append(b)
    vol = bm.calc_volume(signed=True); bm.free()
    return nm, parts, vol

def drop_slivers(ob, min_verts=200):
    """xoa cac manh vun rat nho do boolean de lai"""
    bm = bmesh.new(); bm.from_mesh(ob.data)
    seen = set(); kill = []
    for v in bm.verts:
        if v in seen: continue
        st = [v]; seen.add(v); grp = [v]
        while st:
            a = st.pop()
            for e in a.link_edges:
                b = e.other_vert(a)
                if b not in seen: seen.add(b); st.append(b); grp.append(b)
        if len(grp) < min_verts: kill += grp
    if kill: bmesh.ops.delete(bm, geom=kill, context='VERTS')
    bm.to_mesh(ob.data); bm.free()
    if kill: print("DROP", ob.name, len(kill), "dinh vun")

def clearance(ob, b, n=6):
    """khoang cach nho nhat tu mat hop linh kien toi vat the (mm)"""
    from mathutils.bvhtree import BVHTree
    dg = bpy.context.evaluated_depsgraph_get()
    tree = BVHTree.FromObject(ob, dg)
    best = 1e9
    xs = [b[0] + (b[1] - b[0]) * i / n for i in range(n + 1)]
    ys = [b[2] + (b[3] - b[2]) * i / (2 * n) for i in range(2 * n + 1)]
    zs = [b[4] + (b[5] - b[4]) * i / n for i in range(n + 1)]
    pts = [(x, y, z) for x in xs for y in ys for z in (b[4], b[5])] + \
          [(x, y, z) for x in xs for z in zs for y in (b[2], b[3])] + \
          [(x, y, z) for y in ys for z in zs for x in (b[0], b[1])]
    for p in pts:
        r = tree.find_nearest(Vector(p))
        if r[0] is not None: best = min(best, r[3])
    return best

def collides(ob, b):
    """b = hop (x0,x1,y0,y1,z0,z1) hoac object (se bi xoa)"""
    c = b if isinstance(b, bpy.types.Object) else box("chk", *b)
    t = dup(ob, "chk_t")
    boolean(t, c, 'INTERSECT')
    n = len(t.data.vertices); bpy.data.objects.remove(t)
    return n > 0

def build_parts():
    cage, outer = outer_shell_shape()
    cage_noe = build_cage("Breadcat_cage_long_rong", with_ears=False, deep=True)
    noe = bake(cage_noe, "noe"); flatten(noe, bottom=False)
    inner = sdf_offset(noe, -T, "inner")
    skin = sdf_offset(noe, -EAR_SKIN, "skin")
    plate_src = sdf_offset(noe, -T - 0.2, "plate_src")
    print("BBOX inner", [round(v, 2) for v in bbox(inner)])

    # ---- tai: phan vo ngoai (ke ca lop da 1 mm) nam trong vung lang tru quanh tai, tren duong ghep
    ears = []
    body = dup(outer, "vo_than")
    for s in (1, -1):
        pr = box("prism", min(s * 5.5, s * 40), max(s * 5.5, s * 40), -5, 17.0, EAR_SEAM_Z, 90)
        bv = pr.modifiers.new("Bevel", "BEVEL"); bv.width = 3.0; bv.segments = 4
        cutter = bake(pr, "ear_cutter"); bpy.data.objects.remove(pr)
        boolean(cutter, dup(skin, "sk"), 'DIFFERENCE')          # vung tai = lang tru tru phan ben trong lop da
        e = dup(outer, "ear_R" if s > 0 else "ear_L")
        boolean(e, cutter, 'INTERSECT', keep=True)
        boolean(body, cutter, 'DIFFERENCE')
        ears.append(e)
    boolean(body, dup(inner, "in"), 'DIFFERENCE')
    # tai rong ben trong (thanh 1.2 cho LED xuyen sang) + lo LED
    for s, e in zip((1, -1), ears):
        cav = sdf_offset(e, -1.2, "ear_cav")
        boolean(e, cav, 'DIFFERENCE')
        boolean(e, cyl("bore", (s * LED['x'], LED['y'], LED['z0']), (s * LED['x'], LED['y'], LED['z1']), LED['bore'] / 2), 'DIFFERENCE')

    # ---- tru vit (dinh vao thanh ben bang gan), chi giu phan trong vo
    for (bx, by) in BOSSES:
        bs = cyl("boss", (bx, by, T - 0.01), (bx, by, PCB_Z), BOSS_D / 2)
        sx = 1 if bx > 0 else -1
        rib = box("rib", min(bx, bx + sx * 8), max(bx, bx + sx * 8), by - 1.5, by + 1.5, T - 0.01, PCB_Z)
        boolean(bs, rib, 'UNION'); boolean(bs, dup(outer, "n"), 'INTERSECT')
        boolean(body, bs, 'UNION')
    # ---- module man hinh: 4 tru vit M1.6 o 4 goc PCB, cao bang do day LCD (PCB tua len dau tru)
    c = screw_c()
    for hx in (-1, 1):
        for hz in (MOD_Z0 + c, MOD_Z0 + SCR['pcb_h'] - c):
            x = hx * (SCR['pcb_w'] / 2 - c)
            boolean(body, cyl("post", (x, T - 0.2, hz), (x, T + SCR['lcd_t'], hz), 1.75, 24), 'UNION')
            boolean(body, cyl("pilot", (x, T - 1.0, hz), (x, T + SCR['lcd_t'] + 0.5, hz), 0.65, 16), 'DIFFERENCE')
    # ---- cua so man hinh: ranh dat mica (mat ngoai, sau LENS['t']) + o lo man hinh xuyen qua bac do
    zc_win = WIN_Z0 + WIN / 2                     # tam cua so theo z
    a, b0, b1 = WIN / 2 + WIN_CLR, WIN_Z0 - WIN_CLR, WIN_Z0 + WIN + WIN_CLR
    boolean(body, box("win", -a, a, -1.0, T + 1, b0, b1))                       # o lo xuyen thanh
    rw, rh = LENS['w'] / 2 + LENS['clr'], LENS['h'] / 2 + LENS['clr']
    boolean(body, box("lens_seat", -rw, rw, -1.0, LENS['t'], zc_win - rh, zc_win + rh))   # ranh mica
    # ---- hoc cam ung duoi mai (con 1 mm)
    tz = min(ray_hit(outer, (dx, TOUCH['yc'] + dy, 100), (0, 0, -1)).z
             for dx in (-TOUCH['w'] / 2, 0, TOUCH['w'] / 2) for dy in (-TOUCH['l'] / 2, 0, TOUCH['l'] / 2))
    tw, tl, cl = TOUCH['w'] / 2 + TOUCH['clr'], TOUCH['l'] / 2 + TOUCH['clr'], TOUCH['clr']
    boolean(body, box("touch", -tw, tw, TOUCH['yc'] - tl, TOUCH['yc'] + tl, tz - TOUCH['wall'] - 6, tz - TOUCH['wall']))
    TOUCH['ztop'] = tz - TOUCH['wall']
    # ---- loa: tim mat trong thanh phai o vung loa
    xw = min(ray_hit(body, (0, y, z), (1, 0, 0)).x
             for y in (SPK['y0'] + 1, SPK['y0'] + SPK['l'] / 2, SPK['y0'] + SPK['l'] - 1)
             for z in (SPK['z0'] + 0.5, SPK['z0'] + SPK['h'] / 2, SPK['z0'] + SPK['h'] - 0.5))
    SPK['x1'] = xw - 0.3; SPK['x0'] = SPK['x1'] - SPK['t']
    for zz in (19.5, 21.8, 24.1):
        for i in range(8):
            yy = SPK['y0'] + SPK['l'] / 2 + (i - 3.5) * 2.8
            boolean(body, cyl("grille", (HW - 6, yy, zz), (HW + 6, yy, zz), 0.8, 16))
    # ---- mat sau: USB-C, cong tac, LED sac
    yr = L - T - 1.0
    boolean(body, slot_y("usb", USB['x'], USB['z'], USB['slot'][0], USB['slot'][1], yr, L + 1))
    boolean(body, slot_y("sw", SW['x'], SW['z'], SW['slot'][0], SW['slot'][1], yr, L + 1))
    boolean(body, cyl("d2", (D2['x'], yr, USB['z']), (D2['x'], L + 1, USB['z']), D2['d'] / 2, 20))
    # ---- lo LED tai xuyen than
    for s in (1, -1):
        boolean(body, cyl("bore", (s * LED['x'], LED['y'], LED['z0']), (s * LED['x'], LED['y'], LED['z1']), LED['bore'] / 2))
    # ---- lo moi vit trong tru
    for (bx, by) in BOSSES:
        boolean(body, cyl("pil", (bx, by, T - 0.5), (bx, by, PCB_Z + 0.5), PILOT_D / 2, 20))

    # ---- tam day
    base = dup(plate_src, "tam_day")
    boolean(base, box("slab", -60, 60, -10, 80, 0.0, T), 'INTERSECT')
    for (bx, by) in BOSSES:
        boolean(base, cyl("h", (bx, by, -1), (bx, by, T + 1), 1.15, 24))
        boolean(base, cone("cs", (bx, by, -0.01), 2.2, (bx, by, 1.1), 1.15, 24))
    for sx in (-1, 1):
        rib = box("brib", min(sx * 15.4, sx * 16.4), max(sx * 15.4, sx * 16.4), 17, 52, T - 0.01, T + 2.0)
        boolean(base, rib, 'UNION')

    for o in (inner, skin, plate_src, noe): bpy.data.objects.remove(o)
    for o in [body, base] + ears: drop_slivers(o)
    return dict(cage=cage, cage_noe=cage_noe, outer=outer, body=body, base=base, ear_R=ears[0], ear_L=ears[1])

def bbox(ob):
    vs = [v.co for v in ob.data.vertices]
    return (min(v.x for v in vs), max(v.x for v in vs), min(v.y for v in vs), max(v.y for v in vs), min(v.z for v in vs), max(v.z for v in vs))

def screw_c():
    """tam lo vit cach mep PCB (mm)"""
    return SCR['hole_edge'] + SCR['hole_d'] / 2

def components():
    mz0 = MOD_Z0
    return dict(
        # khoi LCD ap vao mat trong thanh truoc; tam mach PCB nam sau LCD
        man_hinh_lcd=(-SCR['lcd_w'] / 2, SCR['lcd_w'] / 2, T + 0.05, T + SCR['lcd_t'], LCD_Z0, LCD_Z0 + SCR['lcd_h']),
        man_hinh_pcb=(-SCR['pcb_w'] / 2, SCR['pcb_w'] / 2, T + SCR['lcd_t'] + 0.05, T + SCR['lcd_t'] + SCR['pcb_t'], mz0 + 0.05, mz0 + SCR['pcb_h']),
        pin=BAT,
        pcb=(-13.0, 13.0, PCB_Y[0], PCB_Y[1], PCB_Z, PCB_Z + 1.6),      # vung loi PCB (vien that lay theo duong bao xuat ra)
        loa=(SPK['x0'], SPK['x1'], SPK['y0'], SPK['y0'] + SPK['l'], SPK['z0'], SPK['z0'] + SPK['h']),
        cam_ung=(-TOUCH['w'] / 2, TOUCH['w'] / 2, TOUCH['yc'] - TOUCH['l'] / 2, TOUCH['yc'] + TOUCH['l'] / 2, TOUCH['ztop'] - TOUCH['t'], TOUCH['ztop'] - 0.05),
    )

def pcb_outline(body, shrink=0.5, n=180):
    """duong bao PCB = mat trong cua vo o 2 mat PCB (z=12 va 13.6), ban tia tu tam ra, lay tia ngan hon, co vao 0.5 mm;
    phia truoc cat thang tai PCB_Y[0] (sau module man hinh)"""
    c = Vector((0.0, (PCB_Y[0] + PCB_Y[1]) / 2, 0.0))
    out = []
    for i in range(n):
        a = 2 * math.pi * i / n; d = Vector((math.cos(a), math.sin(a), 0.0))
        rs = []
        for z in (PCB_Z + 0.05, PCB_Z + 1.55):
            h = ray_hit(body, (c.x, c.y, z), d)
            # tia lot qua lo USB o thanh sau -> lay mat trong thanh sau
            rs.append((h - Vector((c.x, c.y, z))).length if h else (L - T - c.y) / d.y)
        r = min(rs)
        p = c + d * (r - shrink)
        out.append((p.x, min(max(p.y, PCB_Y[0]), PCB_Y[1])))
    return out

def write_dxf(path, poly, holes):
    L_ = ["0", "SECTION", "2", "ENTITIES"]
    L_ += ["0", "LWPOLYLINE", "8", "Edge.Cuts", "90", str(len(poly)), "70", "1"]
    for x, y in poly: L_ += ["10", f"{x:.3f}", "20", f"{-y:.3f}"]
    for (x, y, d) in holes: L_ += ["0", "CIRCLE", "8", "Edge.Cuts", "10", f"{x:.3f}", "20", f"{-y:.3f}", "40", f"{d / 2:.3f}"]
    L_ += ["0", "ENDSEC", "0", "EOF"]
    open(path, "w").write("\n".join(L_))

def setup_ui_for_user():
    """de mo file .blend la thay ngay ca con meo: don vi mm, goc nhin bao tron mo hinh, to mau theo vat the"""
    us = bpy.context.scene.unit_settings
    us.system = 'METRIC'; us.scale_length = 0.001; us.length_unit = 'MILLIMETERS'
    target = Vector((0.0, 31.0, 28.0)); eye = Vector((-150.0, -230.0, 150.0))
    rot = (target - eye).to_track_quat('-Z', 'Y')
    for scr in bpy.data.screens:
        for area in scr.areas:
            if area.type != 'VIEW_3D': continue
            for sp in area.spaces:
                if sp.type != 'VIEW_3D': continue
                sp.clip_start = 0.1; sp.clip_end = 5000.0
                sp.shading.type = 'SOLID'; sp.shading.color_type = 'OBJECT'
                sp.overlay.grid_scale = 1.0
                r3 = sp.region_3d
                r3.view_location = target; r3.view_rotation = rot; r3.view_distance = 190.0
                r3.view_perspective = 'PERSP'
    for o in bpy.context.scene.objects: o.select_set(False)

def export_stl(ob, path):
    for o in bpy.context.scene.objects: o.select_set(False)
    ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.wm.stl_export(filepath=path, export_selected_objects=True, apply_modifiers=True)

# ---------------------------------------------------------------- main
bpy.ops.wm.read_factory_settings(use_empty=True)
os.makedirs(OUT, exist_ok=True)

if STAGE == "shape":
    cage, outer = outer_shell_shape()
    cage.hide_render = True
    outer.data.materials.clear(); outer.data.materials.append(mat_toast())
    for p in outer.data.polygons: p.use_smooth = True
    face_decals()
    cam = setup_scene()
    bb = [outer.matrix_world @ Vector(c) for c in outer.bound_box]
    print("DIMS", round(max(v.x for v in bb) - min(v.x for v in bb), 1), round(max(v.y for v in bb) - min(v.y for v in bb), 1), round(max(v.z for v in bb), 1))
    for z in (0.3, 3, 5, 8, 10, 20, 30, 38, 42, 44):
        a = ray_hit(outer, (-80, 30, z), (1, 0, 0)); b = ray_hit(outer, (80, 30, z), (-1, 0, 0))
        print("RONG z=%g: %.2f" % (z, b.x - a.x))
    TG = (0, 30, 24)
    views = dict(v34=((-150, -260, 170), TG), front=((0, -330, 60), (0, 30, 28)),
                 side=((300, 30, 60), (0, 31, 28)), back=((170, 290, 170), TG),
                 rear=((0, 360, 70), (0, 31, 32)))
    for k in os.environ.get("BC_VIEWS", "v34,front,side,back").split(","):
        shot(cam, views[k][0], views[k][1], os.path.join(OUT, f"shape_{k}.png"))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "breadcat_shape.blend"))

if STAGE == "mech":
    import time
    t0 = time.time()
    P = build_parts()
    print("BUILD", round(time.time() - t0), "s")
    body, base = P['body'], P['base']
    # ---- kiem tra luoi
    rep = []
    for k in ("body", "base", "ear_R", "ear_L"):
        nm, parts, vol = mesh_stats(P[k])
        rep.append(f"{k}: canh_hong={nm} manh={parts} the_tich={vol / 1000:.1f}cm3")
    # ---- kiem tra linh kien
    C = components()
    for k, b in C.items():
        hit_b = collides(body, b); hit_p = collides(base, b)
        cl = min(clearance(body, b), clearance(base, b))
        rep.append(f"{k}: cham_than={hit_b} cham_day={hit_p} khe_min={cl:.2f}")
    # dau cam USB-C (than nhua 12.5x7 GIA DINH) cam tu ngoai vao
    plug = slot_y("plug", USB['x'], USB['z'], USB['plug'][0], USB['plug'][1], L - T - 0.5, L + 8)
    rep.append(f"usb_dau_cam: cham_than={collides(body, plug)}")
    # LED 3 mm (cao 5.3) nho len tu mieng lo vao hoc tai
    for s, ek in ((1, "ear_R"), (-1, "ear_L")):
        led = cyl("ledchk", (s * LED['x'], LED['y'], LED['z1']), (s * LED['x'], LED['y'], LED['z1'] + 5.3), 1.5, 24)
        rep.append(f"LED_{ek}: cham_tai={collides(P[ek], led)}")
    bo = bbox(P['outer'])
    rep.append(f"KICH_THUOC ngoai: {bo[1] - bo[0]:.1f} rong x {bo[3] - bo[2]:.1f} dai x {bo[5]:.1f} cao (ke ca tai, duoi)")
    def width_at(z, y=30.0):
        a = ray_hit(P['outer'], (-80, y, z), (1, 0, 0)); b = ray_hit(P['outer'], (80, y, z), (-1, 0, 0))
        return b.x - a.x
    zt = max(ray_hit(P['outer'], (0, y, 100), (0, 0, -1)).z for y in (20, 30, 40))
    rep.append(f"DO_THAT: than rong day {max(width_at(z) for z in (3, 5, 8)):.1f} / dinh thanh (z=38) {width_at(38):.1f} mm,"
               f" dinh vom {zt:.1f} mm, dai than {L:.1f} mm")
    rep.append(f"LOA x={SPK['x0']:.1f}..{SPK['x1']:.1f}  CAM_UNG dinh z={TOUCH['ztop']:.1f}")
    rep.append(f"MAN_HINH: PCB {SCR['pcb_w']} x {SCR['pcb_h']} x {SCR['pcb_t']}, LCD {SCR['lcd_w']} x {SCR['lcd_h']} x {SCR['lcd_t']} z {LCD_Z0:.1f}..{LCD_Z0 + SCR['lcd_h']:.1f},"
               f" hien thi {WIN} z {WIN_Z0:.1f}..{WIN_Z0 + WIN:.1f}, 4 lo vit tam cach mep PCB {screw_c():.2f} (x +-{SCR['pcb_w'] / 2 - screw_c():.2f})")
    rep.append(f"MICA: tam {LENS['w']:.1f} x {LENS['h']:.1f} x {LENS['t']:.1f} mm, ranh {LENS['w'] + 2 * LENS['clr']:.2f} x {LENS['h'] + 2 * LENS['clr']:.2f},"
               f" bac do {(LENS['w'] + 2 * LENS['clr'] - WIN - 2 * WIN_CLR) / 2:.2f} mm/ben day {T - LENS['t']:.1f} mm, o lo man hinh {WIN + 2 * WIN_CLR:.2f} mm,"
               f" z {WIN_Z0 - WIN_CLR:.1f}..{WIN_Z0 + WIN + WIN_CLR:.1f}")
    # ---- duong bao PCB -> DXF cho KiCad
    poly = pcb_outline(body)
    xs = [p[0] for p in poly]; ys = [p[1] for p in poly]
    rep.append(f"PCB bao: x {min(xs):.1f}..{max(xs):.1f}  y {min(ys):.1f}..{max(ys):.1f}")
    write_dxf(os.path.join(OUT, "pcb_outline.dxf"), poly, [(bx, by, 2.2) for bx, by in BOSSES])
    print("REPORT\n" + "\n".join(rep))
    open(os.path.join(OUT, "kiem_tra.txt"), "w", encoding="utf-8").write("\n".join(rep) + "\n")
    # ---- xuat STL
    for k, fn in (("body", "vo_than.stl"), ("base", "tam_day.stl"), ("ear_R", "tai_phai.stl"), ("ear_L", "tai_trai.stl")):
        export_stl(P[k], os.path.join(OUT, fn))
    # ---- sap xep file .blend
    cols = {}
    CAGE_COL = "Khung dang (chi xem - sua thong so trong breadcat.py)"
    for nm in (CAGE_COL, "In 3D", "Linh kien (tham khao)"):
        cols[nm] = bpy.data.collections.new(nm); bpy.context.scene.collection.children.link(cols[nm])
    for k in ("cage", "cage_noe"): link(P[k], cols[CAGE_COL])
    P['cage_noe'].hide_viewport = True; P['cage_noe'].hide_render = True
    for k in ("body", "base", "ear_R", "ear_L"): link(P[k], cols["In 3D"])
    P['ear_R'].name = "tai_phai"; P['ear_L'].name = "tai_trai"
    bpy.data.objects.remove(P['outer'])
    P['cage'].hide_render = True
    P['cage'].hide_set(True)             # khung trung voi mat ngoai than -> an de khoi nhap nhay
    colors = dict(man_hinh_lcd=(0.1, 0.15, 0.3, 1), man_hinh_pcb=(0.2, 0.45, 0.85, 1), pin=(0.95, 0.6, 0.2, 1),
                  pcb=(0.15, 0.6, 0.3, 1), loa=(0.6, 0.3, 0.75, 1), cam_ung=(0.9, 0.2, 0.2, 1))
    comps = []
    for k, b in C.items():
        o = box(k, *b); o.color = colors[k]; link(o, cols["Linh kien (tham khao)"]); comps.append(o)
    # vung hien thi 25.6 x 25.6 (lo ra truoc mat LCD ~0.1 mm) + 4 lo vit tren PCB
    disp = box("man_hinh_hien_thi", -WIN / 2, WIN / 2, T - 0.1, T, WIN_Z0, WIN_Z0 + WIN)
    disp.color = (0.02, 0.05, 0.1, 1); link(disp, cols["Linh kien (tham khao)"]); comps.append(disp)
    c = screw_c()
    for hx in (-1, 1):
        for hz in (MOD_Z0 + c, MOD_Z0 + SCR['pcb_h'] - c):
            hole = cyl("man_hinh_lo_vit", (hx * (SCR['pcb_w'] / 2 - c), T + SCR['lcd_t'] - 0.02, hz),
                       (hx * (SCR['pcb_w'] / 2 - c), T + SCR['lcd_t'] + SCR['pcb_t'] + 0.02, hz), SCR['hole_d'] / 2, 16)
            hole.color = (1, 1, 1, 1); link(hole, cols["Linh kien (tham khao)"]); comps.append(hole)
    zcw = WIN_Z0 + WIN / 2
    mica = box("mica", -LENS['w'] / 2, LENS['w'] / 2, 0.0, LENS['t'], zcw - LENS['h'] / 2, zcw + LENS['h'] / 2)
    mica.color = (0.6, 0.85, 1.0, 1); link(mica, cols["Linh kien (tham khao)"]); comps.append(mica)
    # PCB that theo duong bao
    bm = bmesh.new(); vs = [bm.verts.new((x, y, PCB_Z)) for x, y in poly]; f = bm.faces.new(vs)
    ex = bmesh.ops.extrude_face_region(bm, geom=[f]); bmesh.ops.translate(bm, vec=(0, 0, 1.6), verts=[v for v in ex['geom'] if isinstance(v, bmesh.types.BMVert)])
    me = bpy.data.meshes.new("pcb_bao"); bm.to_mesh(me); bm.free()
    pcbo = bpy.data.objects.new("pcb_bao", me); pcbo.color = (0.15, 0.6, 0.3, 1); cols["Linh kien (tham khao)"].objects.link(pcbo)
    for s in (1, -1):
        led = cyl("LED_tai", (s * LED['x'], LED['y'], LED['z1'] - 0.5), (s * LED['x'], LED['y'], LED['z1'] + 4.8), 1.5, 16)
        led.color = (1, 0.9, 0.2, 1); link(led, cols["Linh kien (tham khao)"])
    body.color = (0.93, 0.80, 0.62, 1); base.color = (0.75, 0.72, 0.68, 1)
    for k in ("ear_R", "ear_L"): P[k].color = (0.97, 0.72, 0.62, 1)
    setup_ui_for_user()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "breadcat_vo.blend"))
    print("SAVED", round(time.time() - t0), "s")

    # ---- anh ky thuat (Workbench)
    sc = bpy.context.scene
    sc.render.engine = 'BLENDER_WORKBENCH'
    sh = sc.display.shading; sh.light = 'STUDIO'; sh.color_type = 'OBJECT'; sh.show_cavity = True
    sc.render.film_transparent = False
    wb = bpy.data.worlds.new("wb"); wb.color = (0.92, 0.92, 0.94); sc.world = wb
    sc.render.resolution_x, sc.render.resolution_y = 1000, 760
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    cam.data.lens = 85; cam.data.clip_start = 1; cam.data.clip_end = 5000
    pcbc = [o for o in comps if o.name.startswith("pcb")][0]; pcbc.hide_render = True
    # 1) mat cat doc x=0 nhin tu trai
    cut = dup(body, "cut"); cut.color = body.color
    boolean(cut, box("half", -80, 0.0, -50, 120, -10, 120))
    cutb = dup(base, "cutb"); cutb.color = base.color
    boolean(cutb, box("half", -80, 0.0, -50, 120, -10, 120))
    cute = dup(P['ear_R'], "cute"); cute.color = P['ear_R'].color
    hide = [body, base, P['ear_L'], P['ear_R'], P['cage']]
    for o in hide: o.hide_render = True
    shot(cam, (-320, 30, 30), (0, 31, 28), os.path.join(OUT, "ky_thuat_cat_doc.png"))
    for o in (cut, cutb, cute): bpy.data.objects.remove(o)
    # 2) mat cat ngang z=15 nhin tu tren
    cut = dup(body, "cut"); cut.color = body.color
    boolean(cut, box("top", -80, 80, -50, 120, 20.0, 120))
    shot(cam, (0.01, 31, 330), (0, 31, 0), os.path.join(OUT, "ky_thuat_cat_ngang.png"))
    bpy.data.objects.remove(cut)
    for o in hide: o.hide_render = False
    P['cage'].hide_render = True
    for o in comps + [pcbo]: o.hide_render = True
    # 3) no tung
    base.location.z -= 25; P['ear_R'].location.z += 18; P['ear_L'].location.z += 18
    shot(cam, (-190, -250, 150), (0, 31, 22), os.path.join(OUT, "ky_thuat_no_tung.png"))
    shot(cam, (170, 290, 90), (0, 31, 22), os.path.join(OUT, "ky_thuat_mat_sau.png"))
    base.location.z += 25; P['ear_R'].location.z -= 18; P['ear_L'].location.z -= 18
    shot(cam, (60, 20, -300), (0, 31, 10), os.path.join(OUT, "ky_thuat_day.png"))
    print("DONE", round(time.time() - t0), "s")
