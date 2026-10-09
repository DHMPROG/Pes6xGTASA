"""Trouve les triplets float (x, y, z) proches du ballon dans chaque copie F11.
Le porteur du ballon (actor_id) doit avoir sa position à moins de ~300 unités du ballon."""
import sys
import numpy as np

DUMP_DIR = sys.argv[1]
BEGIN = 0x00BC4000

def meta(i):
    m = {}
    for line in open(f"{DUMP_DIR}/dump{i:02d}.txt"):
        k, _, v = line.strip().partition("=")
        m[k] = v
    return m

for i in (2, 3):
    m = meta(i)
    bx, by, bz = map(float, m["ball_pos"].split())
    d = np.fromfile(f"{DUMP_DIR}/dump{i:02d}.bin", dtype=np.uint8)
    f = d[: len(d) // 4 * 4].view(np.float32)
    x, z = f[:-2], f[2:]
    with np.errstate(invalid="ignore"):
        hit = (np.abs(x - bx) < 300) & (np.abs(z - bz) < 300) & (np.abs(f[1:-1]) < 3000)
    addrs = np.nonzero(hit)[0] * 4 + BEGIN
    print(f"copie {i} : ballon ({bx:.0f},{by:.0f},{bz:.0f}) porteur id {m['actor_id']} -> {len(addrs)} triplets proches")
    for a in addrs[:80]:
        o = a - BEGIN
        print(f"   {a:#010x}  ({f[o//4]:9.1f} {f[o//4+1]:7.1f} {f[o//4+2]:9.1f})")
