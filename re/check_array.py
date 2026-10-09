"""Vérifie l'hypothèse : position du joueur k = 0x03BDE0E0 + (k - 10) * 0x240."""
import sys, struct

DUMP_DIR = sys.argv[1]
BEGIN = 0x00BC4000
STRIDE = 0x240
POS0 = 0x03BDE0E0 - 10 * STRIDE
dumps = [open(f"{DUMP_DIR}/dump{i:02d}.bin", "rb").read() for i in (1, 2, 3)]

def rd(d, addr, fmt):
    return struct.unpack_from(fmt, d, addr - BEGIN)

print("pos joueur 0 =", hex(POS0))
for k in range(32):
    a = POS0 + k * STRIDE
    pos = [rd(d, a, "<3f") for d in dumps]
    # recherche d'un octet == k dans les 0x240 octets qui précèdent (champ id ?)
    print(f"{k:2d} {a:#010x} " + " ".join(f"({x:8.0f},{y:4.0f},{z:7.0f})" for x, y, z in pos))
