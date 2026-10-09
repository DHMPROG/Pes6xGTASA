"""Cherche dans les fiches joueur (pas 0x240) un champ qui marque le joueur contrôlé.
Copie 2 : le joueur contrôlé est le n°10 (il court avec le ballon). Copie 3 : un seul autre joueur de l'équipe A."""
import sys, struct
from collections import Counter

DUMP_DIR = sys.argv[1]
BEGIN = 0x00BC4000
STRIDE = 0x240
POS0 = 0x03BDCA60  # position du joueur 0
dumps = [open(f"{DUMP_DIR}/dump{i:02d}.bin", "rb").read() for i in (1, 2, 3)]
TEAM_A = range(1, 12)

def byte(d, slot, rel):
    return d[POS0 + slot * STRIDE + rel - BEGIN]

for rel in range(-0x120, 0x120):
    res = []
    for d in dumps:
        vals = [byte(d, s, rel) for s in TEAM_A]
        common = Counter(vals).most_common(1)[0][0]
        odd = [s for s, v in zip(TEAM_A, vals) if v != common]
        res.append((odd, [byte(d, s, rel) for s in odd], common))
    (o1, v1, c1), (o2, v2, c2), (o3, v3, c3) = res
    if len(o2) == 1 and o2[0] == 10 and len(o3) == 1 and o3[0] != 10 and len(o1) == 1:
        print(f"pos{rel:+#06x}: copie1 n°{o1}={v1} (autres {c1})  copie2 n°{o2}={v2} (autres {c2})  copie3 n°{o3}={v3} (autres {c3})")
