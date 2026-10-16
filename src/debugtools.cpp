// Outils de rétro-ingénierie : panneau de valeurs en direct (F12) et copie de la mémoire (F11).
#include "debugtools.h"
#include "pes.h"
#include "imgui.h"
#include <cstdio>
#include <vector>

void Log(const char* fmt, ...);

static int g_dumpIndex = 0;

struct Snapshot {
    uintptr_t ball = 0, active = 0;
    pes::Vec3 ballPos = {}, activePos = {};
    uint8_t ballState = 0xFF, actorId = 0xFF, activeId = 0xFF;
    bool hasBall = false, hasActive = false;
};

static Snapshot Take() {
    Snapshot s;
    if (pes::Read(pes::kBallPtr, s.ball) && s.ball && pes::Read(s.ball + pes::kBallPos, s.ballPos)) {
        s.hasBall = true;
        pes::Read(s.ball + pes::kBallState, s.ballState);
    }
    pes::Read(pes::kBallActorId, s.actorId);
    if (pes::Read(pes::kActivePlayer, s.active) && pes::LooksLikePlayer(s.active)) {
        s.hasActive = pes::Read(s.active + pes::kPlayerPos, s.activePos);
        pes::Read(s.active + pes::kPlayerId, s.activeId);
    }
    return s;
}


// ---------------------------------------------------------------- recherche du joueur contrôlé
// Hypothèse PES : quand ton équipe a le ballon, c'est toi qui le contrôles.
// On suit, pour chaque équipe, les octets de .data qui :
//   - restent dans la plage des numéros de cette équipe pendant le jeu ;
//   - valent le numéro du porteur quand un joueur de cette équipe conduit le ballon (état 0)
//     depuis au moins 0,5 s.
// L'équipe humaine est celle dont la liste ne tombe pas à zéro.
struct Finder {
    const char* name;
    int lo, hi;                      // plage des numéros de l'équipe
    std::vector<uint32_t> cand;      // décalages dans .data
    bool init = false;
    size_t lastLogged = 0;
};
static Finder g_finders[2] = { { "A", 1, 11 }, { "B", 12, 22 } };
static int g_stableActor = -1, g_stableTicks = 0;
static ULONGLONG g_lastTick = 0;

static void FinderInit(Finder& f) {
    const uint8_t* mem = (const uint8_t*)pes::kDataBegin;
    size_t n = pes::kDataEnd - pes::kDataBegin;
    f.cand.clear();
    for (size_t i = 0; i < n; i++)
        if (mem[i] >= f.lo && mem[i] <= f.hi) f.cand.push_back((uint32_t)i);
    f.init = true;
    f.lastLogged = 0;
    Log("Recherche joueur controle, equipe %s : %u candidats", f.name, (unsigned)f.cand.size());
}

static void FinderFilter(Finder& f, int mustEqual) {
    const uint8_t* mem = (const uint8_t*)pes::kDataBegin;
    size_t out = 0;
    for (uint32_t off : f.cand) {
        uint8_t v = mem[off];
        bool keep = v >= f.lo && v <= f.hi && (mustEqual < 0 || v == mustEqual);
        if (keep) f.cand[out++] = off;
    }
    f.cand.resize(out);
    if (f.cand.size() <= 32 && f.cand.size() != f.lastLogged) {
        f.lastLogged = f.cand.size();
        char line[1024];
        int len = wsprintfA(line, "Equipe %s : %u candidats :", f.name, (unsigned)f.cand.size());
        for (uint32_t off : f.cand)
            if (len < 960) len += wsprintfA(line + len, " %08X=%u", (unsigned)(pes::kDataBegin + off), mem[off]);
        Log("%s", line);
    }
}

void Debug_FinderReset() {
    for (Finder& f : g_finders) f.init = false;
    Log("Recherche joueur controle : remise a zero");
}

void Debug_Tick() {
    ULONGLONG now = GetTickCount64();
    if (now - g_lastTick < 100) return;  // 10 fois par seconde
    g_lastTick = now;

    Snapshot s = Take();
    if (!s.hasBall) return;
    // On ne filtre que pendant qu'un joueur conduit le ballon (état 0)
    if (s.ballState != 0 || s.actorId < 1 || s.actorId > 22) { g_stableActor = -1; return; }
    if (s.actorId == g_stableActor) g_stableTicks++;
    else { g_stableActor = s.actorId; g_stableTicks = 0; }
    if (g_stableTicks < 5) return;

    for (Finder& f : g_finders) {
        if (!f.init) FinderInit(f);
        bool ours = s.actorId >= f.lo && s.actorId <= f.hi;
        FinderFilter(f, ours ? s.actorId : -1);
    }
}

static void FinderPanel() {
    const uint8_t* mem = (const uint8_t*)pes::kDataBegin;
    for (Finder& f : g_finders) {
        if (!f.init) { ImGui::Text("Equipe %s : en attente", f.name); continue; }
        ImGui::Text("Equipe %s : %u candidats", f.name, (unsigned)f.cand.size());
        if (f.cand.size() <= 6)
            for (uint32_t off : f.cand) ImGui::Text("   %08X = %u", (unsigned)(pes::kDataBegin + off), mem[off]);
    }
}

void Debug_DrawPanel() {
    Snapshot s = Take();
    ImGui::SetNextWindowPos(ImVec2(20, 120), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("PES6 debug", nullptr,
                 ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text("Ballon      %08X", (unsigned)s.ball);
    if (s.hasBall)
        ImGui::Text("  pos       %8.2f %8.2f %8.2f  etat %u", s.ballPos.x, s.ballPos.y, s.ballPos.z, s.ballState);
    ImGui::Text("Id au ballon %u", s.actorId);
    ImGui::Text("Joueur actif %08X", (unsigned)s.active);
    if (s.hasActive)
        ImGui::Text("  id %2u pos %8.2f %8.2f %8.2f", s.activeId, s.activePos.x, s.activePos.y, s.activePos.z);
    ImGui::Separator();
    for (int i = 1; i < pes::kActorCount; i++) {
        pes::Vec3 p;
        char name[64] = "?";
        if (pes::Read(pes::ActorPos(i), p) && i == s.actorId) {
            pes::ActorName(i, name, sizeof(name));
            ImGui::Text("Porteur n.%d %s : %8.0f %8.0f", i, name, p.x, p.z);
        }
    }
    uint8_t ca = 0, ca2 = 0, cb = 0;
    pes::Read(pes::kCtrlTeamA, ca);
    pes::Read(pes::kCtrlTeamA2, ca2);
    pes::Read(pes::kCtrlTeamB, cb);
    ImGui::Text("Controle A %08X=%u  %08X=%u", (unsigned)pes::kCtrlTeamA, ca, (unsigned)pes::kCtrlTeamA2, ca2);
    ImGui::Text("Controle B %08X=%u", (unsigned)pes::kCtrlTeamB, cb);
    FinderPanel();
    ImGui::Text("F11 : copie memoire (%d faites)", g_dumpIndex);
    ImGui::End();
}

// Copie la section .data de PES6.exe (pages illisibles remplies de zéros) + un fichier texte d'infos.
void Debug_Dump(const char* gameDir, const char* note) {
    char dir[MAX_PATH], path[MAX_PATH];
    wsprintfA(dir, "%s\\pes6fun_dumps", gameDir);
    CreateDirectoryA(dir, nullptr);
    int n = ++g_dumpIndex;
    Snapshot s = Take();

    wsprintfA(path, "%s\\dump%02d.txt", dir, n);
    if (FILE* f = fopen(path, "w")) {
        fprintf(f, "note=%s\nregion_begin=0x%08X\nregion_end=0x%08X\n", note ? note : "",
                (unsigned)pes::kDataBegin, (unsigned)pes::kDataEnd);
        fprintf(f, "ball=0x%08X\nball_pos=%f %f %f\nball_state=%u\nactor_id=%u\n", (unsigned)s.ball,
                s.ballPos.x, s.ballPos.y, s.ballPos.z, s.ballState, s.actorId);
        fprintf(f, "active=0x%08X\nactive_id=%u\nactive_pos=%f %f %f\n", (unsigned)s.active, s.activeId,
                s.activePos.x, s.activePos.y, s.activePos.z);
        fclose(f);
    }

    wsprintfA(path, "%s\\dump%02d.bin", dir, n);
    FILE* f = fopen(path, "wb");
    if (!f) { Log("Dump %d : impossible d'ecrire %s", n, path); return; }
    static char zeros[4096];
    size_t written = 0, skipped = 0;
    for (uintptr_t a = pes::kDataBegin; a < pes::kDataEnd;) {
        MEMORY_BASIC_INFORMATION mbi;
        uintptr_t end = pes::kDataEnd;
        bool ok = VirtualQuery((void*)a, &mbi, sizeof(mbi)) != 0;
        if (ok) {
            uintptr_t regionEnd = (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
            if (regionEnd < end) end = regionEnd;
            ok = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
        }
        if (ok) {
            fwrite((const void*)a, 1, end - a, f);
            written += end - a;
        } else {
            for (uintptr_t z = a; z < end; z += sizeof(zeros))
                fwrite(zeros, 1, (end - z) < sizeof(zeros) ? (end - z) : sizeof(zeros), f);
            skipped += end - a;
        }
        a = end;
    }
    fclose(f);
    Log("Dump %d (%s) : %u Ko lus, %u Ko illisibles, ballon=%08X actif=%08X", n, note ? note : "",
        (unsigned)(written / 1024), (unsigned)(skipped / 1024), (unsigned)s.ball, (unsigned)s.active);
}
