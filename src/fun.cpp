// Mode FUN, première version jouable :
// F10 vise l'adversaire le plus proche du joueur contrôlé et le pousse derrière la ligne de touche.
// On réécrit sa position à chaque image pendant quelques secondes, puis on le relâche.
// Personne n'est supprimé du tableau des acteurs.
#include "fun.h"
#include "pes.h"
#include <cmath>

void Log(const char* fmt, ...);

static int g_human = 0;          // 0 = équipe A (1..11), 1 = équipe B (12..22)
static int g_freezeMs = 15000;
static char g_victim[64] = "";

struct Frozen {
    bool active = false;
    pes::Vec3 pos = {};
    ULONGLONG until = 0, since = 0;
    int frames = 0, overwritten = 0;  // combien de fois le jeu a remis sa propre position
    bool reported = false;
};
static Frozen g_frozen[pes::kActorCount];

static int TeamLo(int team) { return team == 0 ? 1 : 12; }
static int TeamHi(int team) { return team == 0 ? 11 : 22; }
static bool InTeam(int i, int team) { return i >= TeamLo(team) && i <= TeamHi(team); }

void Fun_Init(int humanTeam, int freezeSeconds) {
    g_human = humanTeam ? 1 : 0;
    g_freezeMs = (freezeSeconds > 0 ? freezeSeconds : 15) * 1000;
}

const char* Fun_LastVictim() { return g_victim; }

// Joueur contrôlé de l'équipe humaine. Candidat principal trouvé par le filtre F12,
// sinon le porteur du ballon s'il est de notre équipe.
static int ControlledIndex() {
    uint8_t v;
    if (pes::Read(g_human == 0 ? pes::kCtrlTeamA : pes::kCtrlTeamB, v) && InTeam(v, g_human)) return v;
    if (pes::Read(pes::kBallActorId, v) && InTeam(v, g_human)) return v;
    return -1;
}

static bool ActorPos(int i, pes::Vec3& p) { return pes::Read(pes::ActorPos(i), p); }

bool Fun_Shoot() {
    int me = ControlledIndex();
    pes::Vec3 from;
    if (me < 0 || !ActorPos(me, from)) {
        Log("Tir : joueur controle introuvable");
        return false;
    }
    int foe = 1 - g_human, best = -1;
    float bestD = 1e30f;
    pes::Vec3 bestPos = {};
    for (int i = TeamLo(foe); i <= TeamHi(foe); i++) {
        pes::Vec3 p;
        if (g_frozen[i].active || !ActorPos(i, p)) continue;
        float dx = p.x - from.x, dz = p.z - from.z, d = dx * dx + dz * dz;
        if (d < bestD) { bestD = d; best = i; bestPos = p; }
    }
    if (best < 0) {
        Log("Tir : aucun adversaire disponible");
        return false;
    }
    Frozen& f = g_frozen[best];
    f = Frozen();
    f.active = true;
    // Derrière la ligne de touche la plus proche (touches vers z = ±8500)
    f.pos = { bestPos.x, 0.0f, bestPos.z >= 0 ? 9800.0f : -9800.0f };
    f.since = GetTickCount64();
    f.until = f.since + g_freezeMs;
    wsprintfA(g_victim, "joueur n.%d", best);
    Log("Tir : %d (%.0f, %.0f) -> %d a %.0f unites, pousse en z=%.0f", me, from.x, from.z, best,
        sqrtf(bestD), f.pos.z);
    return true;
}

void Fun_Tick() {
    ULONGLONG now = GetTickCount64();
    for (int i = 0; i < pes::kActorCount; i++) {
        Frozen& f = g_frozen[i];
        if (!f.active) continue;
        uintptr_t a = pes::ActorPos(i);
        pes::Vec3 cur;
        if (!pes::Read(a, cur)) { f.active = false; continue; }
        if (f.frames > 0 && (fabsf(cur.x - f.pos.x) > 1 || fabsf(cur.z - f.pos.z) > 1)) f.overwritten++;
        f.frames++;
        if (!f.reported && now - f.since >= 1000) {
            f.reported = true;
            Log("Neutralise %d : le jeu a remis sa position %d fois sur %d images", i, f.overwritten, f.frames);
        }
        if (now >= f.until) {
            f.active = false;
            Log("Neutralise %d : relache", i);
            continue;
        }
        *(pes::Vec3*)a = f.pos;
    }
}

void Fun_ReleaseAll() {
    for (Frozen& f : g_frozen) f.active = false;
}

// Mini-carte façon GTA en bas à gauche : terrain, joueurs, ballon
void Fun_DrawRadar(ImDrawList* dl, ImVec2 screen) {
    const float halfX = 14500, halfZ = 10500;  // terrain + marge (buts ±13200, touches ±8500)
    float w = screen.x * 0.24f, h = w * halfZ / halfX;
    ImVec2 o(screen.x * 0.02f, screen.y - h - screen.y * 0.03f);
    auto map = [&](float x, float z) { return ImVec2(o.x + (x + halfX) / (2 * halfX) * w, o.y + (z + halfZ) / (2 * halfZ) * h); };

    dl->AddRectFilled(o, ImVec2(o.x + w, o.y + h), IM_COL32(0, 0, 0, 150), 6.0f);
    dl->AddRect(map(-13200, -8500), map(13200, 8500), IM_COL32(255, 255, 255, 110));
    dl->AddLine(map(0, -8500), map(0, 8500), IM_COL32(255, 255, 255, 70));

    int me = ControlledIndex();
    uint8_t cand[2] = { 0, 0 };
    pes::Read(pes::kCtrlTeamA, cand[0]);
    pes::Read(pes::kCtrlTeamB, cand[1]);
    float r = w / 70;
    for (int i = 1; i < pes::kActorCount; i++) {
        pes::Vec3 p;
        if (!ActorPos(i, p)) continue;
        ImVec2 c = map(p.x, p.z);
        ImU32 col = i <= 11 ? IM_COL32(80, 150, 255, 255) : IM_COL32(255, 80, 80, 255);
        if (g_frozen[i].active) col = IM_COL32(120, 120, 120, 255);
        dl->AddCircleFilled(c, r, col);
        if (i == cand[0] || i == cand[1]) dl->AddCircle(c, r * 1.8f, IM_COL32(255, 255, 0, 200), 0, 1.5f);
        if (i == me) dl->AddTriangleFilled(ImVec2(c.x, c.y - r * 3.2f), ImVec2(c.x - r, c.y - r * 1.8f),
                                           ImVec2(c.x + r, c.y - r * 1.8f), IM_COL32(255, 255, 255, 255));
    }
    uintptr_t ball;
    pes::Vec3 bp;
    if (pes::Read(pes::kBallPtr, ball) && ball && pes::Read(ball + pes::kBallPos, bp))
        dl->AddCircleFilled(map(bp.x, bp.z), r * 0.7f, IM_COL32(255, 255, 255, 255));
}
