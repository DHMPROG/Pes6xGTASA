"""Cherche les structures « joueur » de PES 6 dans les copies mémoire F11.

Critères (structure joueur d'après pes6_game_physics_mod) :
  +0x00 u8  id 0..31, +0x04 pointeur non nul, +0xE0/+0xE4/+0xE8 floats position.
On garde les candidats valides dans les 3 copies, puis on cherche un pas régulier.
"""
import sys, struct
import numpy as np

DUMP_DIR = sys.argv[1]
BEGIN = 0x00BC4000
dumps = [np.fromfile(f"{DUMP_DIR}/dump{i:02d}.bin", dtype=np.uint8) for i in (1, 2, 3)]
n = len(dumps[0])

def f32(d, off):  # vue float32 alignée
    return d[off: off + (n - off) // 4 * 4].view(np.float32)
def u32(d, off):
    return d[off: off + (n - off) // 4 * 4].view(np.uint32)

ok = None
for d in dumps:
    idx = np.arange(0, n - 0x100, 4)
    ids = d[idx]
    anim = u32(d, 4)[: len(idx)]
    x = f32(d, 0xE0)[: len(idx)]; y = f32(d, 0xE4)[: len(idx)]; z = f32(d, 0xE8)[: len(idx)]
    good = (ids <= 31) & (anim != 0) & np.isfinite(x) & np.isfinite(y) & np.isfinite(z)
    good &= (np.abs(x) < 70000) & (np.abs(z) < 50000) & (np.abs(y) < 2000)
    good &= (np.abs(x) > 1) | (np.abs(z) > 1)
    ok = good if ok is None else ok & good
cand = np.arange(0, n - 0x100, 4)[ok[: len(np.arange(0, n - 0x100, 4))]]

# la position doit changer entre la copie 1 et la copie 2 ou 3
moved = []
for off in cand:
    p = [struct.unpack_from("<3f", d, off + 0xE0) for d in dumps]
    if any(abs(a - b) > 1 for a, b in zip(p[0], p[1])) or any(abs(a - b) > 1 for a, b in zip(p[0], p[2])):
        moved.append(off)
print(f"{len(cand)} candidats stables, {len(moved)} qui bougent")

# pas le plus fréquent entre candidats qui bougent
moved = np.array(moved)
if len(moved) > 1:
    steps, counts = np.unique(np.diff(moved), return_counts=True)
    order = np.argsort(-counts)[:8]
    print("pas les plus frequents :", [(hex(int(steps[i])), int(counts[i])) for i in order])
for off in moved[:60]:
    p = [struct.unpack_from("<3f", d, off + 0xE0) for d in dumps]
    print(hex(BEGIN + int(off)), "id", dumps[0][off], dumps[2][off], " ".join(f"({a:8.0f},{b:5.0f},{c:8.0f})" for a, b, c in p))
