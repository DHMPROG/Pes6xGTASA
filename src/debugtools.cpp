// Outils de rétro-ingénierie : panneau de valeurs en direct (F12) et copie de la mémoire (F11).
#include "debugtools.h"
#include "pes.h"
#include "imgui.h"
#include <cstdio>

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
