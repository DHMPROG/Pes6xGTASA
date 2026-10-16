// Mode FUN : tir sur les adversaires, joueurs neutralisés, radar
#pragma once
#include "imgui.h"

void Fun_Init(int humanTeam, int freezeSeconds);  // humanTeam : 0 = équipe A, 1 = équipe B
bool Fun_Shoot();                                 // F10 : true si un adversaire a été touché
void Fun_Tick();                                  // à chaque image, maintient les joueurs neutralisés
void Fun_ReleaseAll();                            // libère tout le monde (Mode FUN coupé)
void Fun_DrawRadar(ImDrawList* dl, ImVec2 screen);
const char* Fun_LastVictim();                     // texte affiché sous « WASTED »
