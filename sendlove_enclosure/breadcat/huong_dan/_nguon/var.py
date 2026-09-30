import re, subprocess, os, sys
from concurrent.futures import ThreadPoolExecutor
SRC = r"P:\coddd\sendove-box\sendlove_enclosure\breadcat\breadcat.py"
BL = r"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe"
base = open(SRC, encoding="utf-8").read()
V = {
  "goc":     ([], "front,side,rear"),
  "ear60":   ([("Z_EAR = 64.0", "Z_EAR = 60.0")], "front"),
  "notch49": ([("Z_NOTCH = 45.5", "Z_NOTCH = 49.0")], "front"),
  "dome57":  ([("Z_DOME = 52.5", "Z_DOME = 57.0")], "side"),
  "heart16": ([("HEART_W = 22.0", "HEART_W = 16.0")], "rear"),
  "tailz35": ([("z=41.0, out=3.0", "z=35.0, out=3.0")], "side"),
  "taper0":  ([("TAPER = 3.45", "TAPER = 0.0"), ("W = 45.35", "W = 48.0")], "front"),
  "L72":     ([("L = 62.0 ", "L = 72.0 ")], "side"),
}
def run(k):
    reps, views = V[k]
    s = base
    for a, b in reps:
        assert a in s, (k, a); s = s.replace(a, b, 1)
    d = os.path.abspath(os.path.join("var", k)); os.makedirs(d, exist_ok=True)
    p = os.path.join(d, "bc.py"); open(p, "w", encoding="utf-8").write(s)
    env = dict(os.environ, BC_SAMPLES="16", BC_W="640", BC_VIEWS=views)
    r = subprocess.run([BL, "-b", "--factory-startup", "--python-exit-code", "1", "--python", p, "--", "shape", d], env=env, capture_output=True, text=True, errors="replace")
    dims = [l for l in r.stdout.splitlines() if l.startswith("DIMS")]
    return k, r.returncode, dims
with ThreadPoolExecutor(4) as ex:
    for k, rc, dims in ex.map(run, V):
        print(k, rc, dims)
