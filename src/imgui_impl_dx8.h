// Backend de rendu Dear ImGui pour Direct3D 8 (adapté de imgui_impl_dx9, v1.92).
// Limite : DX8 n'a pas de scissor test, le clipping des fenêtres ImGui est ignoré.
#pragma once
#include "imgui.h"

struct IDirect3DDevice8;

bool ImGui_ImplDX8_Init(IDirect3DDevice8* device);
void ImGui_ImplDX8_Shutdown();
void ImGui_ImplDX8_NewFrame();
void ImGui_ImplDX8_RenderDrawData(ImDrawData* draw_data);

// À appeler autour de IDirect3DDevice8::Reset (libère / recrée les buffers D3DPOOL_DEFAULT)
void ImGui_ImplDX8_InvalidateDeviceObjects();
