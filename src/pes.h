// Adresses de PES6.exe (retail Win32, base 0x400000).
// Source : pes6_game_physics_mod (Angel Ballay, MIT), vérifiées octet par octet
// sur notre PES6.exe le 2026-10-09.
#pragma once
#include <windows.h>
#include <cstdint>

namespace pes {

constexpr uintptr_t kBase = 0x400000;

constexpr uintptr_t kBallPtr       = kBase + 0x7CCE94;   // [ ] -> structure du ballon
constexpr uintptr_t kBallActorId   = kBase + 0x37E09CC;  // u8 : id du joueur qui touche le ballon
constexpr uintptr_t kActivePlayer  = kBase + 0x37E0AA0;  // [ ] -> joueur actif

// Structure du ballon
constexpr uintptr_t kBallPos = 0x20;    // float x, y (hauteur), z
constexpr uintptr_t kBallState = 0x84;  // 0 conduite, 1 passe, 3 centre, 4 tir, 5 arrêté

// Structure d'un joueur
constexpr uintptr_t kPlayerId = 0x00;        // u8, 0..31
constexpr uintptr_t kPlayerAnim = 0x04;      // pointeur animation
constexpr uintptr_t kPlayerPhysics = 0xD0;   // pointeur sous-structure physique (pos à +0)
constexpr uintptr_t kPlayerPos = 0xE0;       // float x, y (vertical), z

// Tableau des acteurs du match (trouvé par nous avec les copies F11, 2026-10-09) :
// 23 fiches de 0x240 octets. 0 = arbitre, 1..11 = équipe A (1 = gardien), 12..22 = équipe B (12 = gardien).
// L'identifiant « porteur du ballon » (kBallActorId) est un index dans ce tableau.
constexpr uintptr_t kActorPos0   = 0x03BDCA60;  // float x, y, z de l'acteur 0 (y = 0 au sol)
constexpr uintptr_t kActorStride = 0x240;
constexpr int kActorCount = 23;
inline uintptr_t ActorPos(int i) { return kActorPos0 + i * kActorStride; }

// Joueur contrôlé, u8 = index dans le tableau des acteurs (filtre F12 du 2026-10-09, à confirmer) :
// équipe A : 0x03BCE05C (autre candidat restant : 0x03BCECD0), équipe B : 0x03BCE04C (16 octets avant).
constexpr uintptr_t kCtrlTeamA = 0x03BCE05C;
constexpr uintptr_t kCtrlTeamA2 = 0x03BCECD0;
constexpr uintptr_t kCtrlTeamB = 0x03BCE04C;

// Section .data (variables globales + BSS) : 0xBC4000 .. 0x3D59000
constexpr uintptr_t kDataBegin = kBase + 0x7C4000;
constexpr uintptr_t kDataEnd   = kBase + 0x7C4000 + 0x3195000;

// Lecture sans crash : vérifie que la page est lisible
inline bool Readable(uintptr_t addr, size_t size) {
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi))) return false;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
    return addr + size <= (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
}

template <typename T>
inline bool Read(uintptr_t addr, T& out) {
    if (!Readable(addr, sizeof(T))) return false;
    out = *(const T*)addr;
    return true;
}

struct Vec3 { float x, y, z; };

inline bool LooksLikePlayer(uintptr_t p) {
    uint8_t id;
    uintptr_t anim;
    return p >= 0x01000000 && p <= 0x08000000 && Read(p + kPlayerId, id) && id <= 31 &&
           Read(p + kPlayerAnim, anim) && anim != 0;
}

}  // namespace pes
