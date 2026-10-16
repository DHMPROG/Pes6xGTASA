// PES6 x GTA SA — « Mode FUN »
// Étape 1 : la DLL se charge, intercepte Present/Reset de D3D8, affiche l'overlay
// et gère le bouton Mode FUN (F9). Quand le mode est OFF, rien n'est dessiné.
#include <windows.h>
#include <d3d8.h>
#include <cstdio>
#include <cstdarg>

#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx8.h"
#include "imgui_impl_win32.h"
#include "debugtools.h"
#include "fun.h"

// ---------------------------------------------------------------- log + config

static char g_dir[MAX_PATH];   // dossier de PES6.exe
static char g_ini[MAX_PATH];
static FILE* g_log = nullptr;

void Log(const char* fmt, ...) {
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_list a;
    va_start(a, fmt);
    vfprintf(g_log, fmt, a);
    va_end(a);
    fputc('\n', g_log);
    fflush(g_log);
}

struct Config {
    bool modeFun = false;
    int toggleKey = VK_F9;
    int testKey = VK_F10;
    int dumpKey = VK_F11;
    int debugKey = VK_F12;
    bool debugPanel = false;
    int humanTeam = 0;     // 0 = équipe A (domicile), 1 = équipe B
    int freezeSeconds = 15;
    int debugRender = 2;   // 0 = hooks seuls, 1 = frame ImGui sans dessin D3D, 2 = overlay complet
};
static Config g_cfg;

static void LoadConfig() {
    g_cfg.modeFun = GetPrivateProfileIntA("General", "ModeFun", 0, g_ini) != 0;
    g_cfg.toggleKey = GetPrivateProfileIntA("Keys", "ToggleModeFun", VK_F9, g_ini);
    g_cfg.testKey = GetPrivateProfileIntA("Keys", "Test", VK_F10, g_ini);
    g_cfg.dumpKey = GetPrivateProfileIntA("Keys", "Dump", VK_F11, g_ini);
    g_cfg.debugKey = GetPrivateProfileIntA("Keys", "DebugPanel", VK_F12, g_ini);
    g_cfg.debugPanel = GetPrivateProfileIntA("Debug", "Panel", 0, g_ini) != 0;
    g_cfg.debugRender = GetPrivateProfileIntA("Debug", "Render", 2, g_ini);
    g_cfg.humanTeam = GetPrivateProfileIntA("General", "HumanTeam", 0, g_ini);
    g_cfg.freezeSeconds = GetPrivateProfileIntA("General", "FreezeSeconds", 15, g_ini);
    Fun_Init(g_cfg.humanTeam, g_cfg.freezeSeconds);
}

static void SaveModeFun() {
    WritePrivateProfileStringA("General", "ModeFun", g_cfg.modeFun ? "1" : "0", g_ini);
}

// ---------------------------------------------------------------- overlay

static HWND g_hwnd = nullptr;
static bool g_imguiReady = false;
static ImFont* g_bigFont = nullptr;
static double g_toastUntil = 0;   // fin du message « MODE FUN : ON/OFF »
static double g_testUntil = 0;    // fin du texte de test (F10)

static double Now() { return GetTickCount64() / 1000.0; }

// Texte avec contour noir, façon HUD de GTA
static void OutlinedText(ImDrawList* dl, ImFont* font, float size, ImVec2 pos, ImU32 col, const char* text) {
    const ImU32 black = IM_COL32(0, 0, 0, (col >> IM_COL32_A_SHIFT) & 0xFF);
    const float o = size / 16.0f;
    for (int dx = -1; dx <= 1; ++dx)
        for (int dy = -1; dy <= 1; ++dy)
            if (dx || dy) dl->AddText(font, size, ImVec2(pos.x + dx * o, pos.y + dy * o), black, text);
    dl->AddText(font, size, pos, col, text);
}

static ImVec2 TextSize(ImFont* font, float size, const char* text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
}

static bool KeyPressed(int vk) {
    static bool down[256];
    bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool pressed = now && !down[vk & 0xFF];
    down[vk & 0xFF] = now;
    return pressed;
}

static void HandleKeys() {
    if (GetForegroundWindow() != g_hwnd) return;
    if (KeyPressed(g_cfg.toggleKey)) {
        g_cfg.modeFun = !g_cfg.modeFun;
        SaveModeFun();
        g_toastUntil = Now() + 2.0;
        if (!g_cfg.modeFun) Fun_ReleaseAll();
        Log("Mode FUN %s", g_cfg.modeFun ? "ON" : "OFF");
    }
    if (KeyPressed(g_cfg.testKey) && g_cfg.modeFun) {
        if (Fun_Shoot()) g_testUntil = Now() + 1.5;
    }
    if (KeyPressed(g_cfg.debugKey)) {
        g_cfg.debugPanel = !g_cfg.debugPanel;
        WritePrivateProfileStringA("Debug", "Panel", g_cfg.debugPanel ? "1" : "0", g_ini);
    }
    if (KeyPressed(g_cfg.dumpKey)) {
        char note[32];
        wsprintfA(note, "F11 #%lu", (unsigned long)(GetTickCount64() / 1000));
        Debug_Dump(g_dir, note);
    }
}

static void DrawOverlay() {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 screen = ImGui::GetIO().DisplaySize;
    ImFont* font = g_bigFont ? g_bigFont : ImGui::GetFont();
    double t = Now();

    // Badge permanent en haut à droite, seulement quand le mode est actif
    if (g_cfg.modeFun) {
        const char* label = "MODE FUN";
        float size = screen.y * 0.045f;
        ImVec2 sz = TextSize(font, size, label);
        OutlinedText(dl, font, size, ImVec2(screen.x - sz.x - screen.x * 0.02f, screen.y * 0.02f),
                     IM_COL32(255, 120, 200, 255), label);
    }

    // Message temporaire après F9
    if (t < g_toastUntil) {
        const char* label = g_cfg.modeFun ? "MODE FUN : ON" : "MODE FUN : OFF";
        float size = screen.y * 0.06f;
        ImVec2 sz = TextSize(font, size, label);
        int alpha = (int)(255 * ((g_toastUntil - t) < 0.5 ? (g_toastUntil - t) / 0.5 : 1.0));
        ImU32 col = g_cfg.modeFun ? IM_COL32(120, 255, 120, alpha) : IM_COL32(230, 230, 230, alpha);
        OutlinedText(dl, font, size, ImVec2((screen.x - sz.x) / 2, screen.y * 0.30f), col, label);
    }

    if (g_cfg.modeFun) {
        Fun_Tick();
        Fun_DrawRadar(dl, screen);
    }

    // Tir (F10) : texte « WASTED » rouge au centre, avec la victime en dessous
    if (g_cfg.modeFun && t < g_testUntil) {
        const char* label = "WASTED";
        float size = screen.y * 0.12f;
        ImVec2 sz = TextSize(font, size, label);
        OutlinedText(dl, font, size, ImVec2((screen.x - sz.x) / 2, (screen.y - sz.y) / 2),
                     IM_COL32(200, 30, 30, 255), label);
        const char* who = Fun_LastVictim();
        float small = screen.y * 0.04f;
        ImVec2 wsz = TextSize(font, small, who);
        OutlinedText(dl, font, small, ImVec2((screen.x - wsz.x) / 2, (screen.y + sz.y) / 2),
                     IM_COL32(240, 240, 240, 255), who);
    }

    if (g_cfg.debugPanel) {
        Debug_Tick();
        Debug_DrawPanel();
    }
}

static void InitImGui(IDirect3DDevice8* dev) {
    D3DDEVICE_CREATION_PARAMETERS cp;
    dev->GetCreationParameters(&cp);
    g_hwnd = cp.hFocusWindow;
    if (!g_hwnd) g_hwnd = FindWindowA(nullptr, "Pro Evolution Soccer 6");

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    // Impact ressemble assez à la police « Pricedown » de GTA
    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    lstrcatA(fontPath, "\\Fonts\\impact.ttf");
    if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES)
        g_bigFont = io.Fonts->AddFontFromFileTTF(fontPath, 32.0f);

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX8_Init(dev);
    g_imguiReady = true;
    Log("ImGui pret (hwnd=%p, police=%s)", (void*)g_hwnd, g_bigFont ? "impact" : "defaut");
}

// ---------------------------------------------------------------- hooks D3D8
// PES6.exe importe d3d8.dll. On remplace Direct3DCreate8 dans sa table d'imports (IAT),
// puis on intercepte CreateDevice pour poser Present/Reset sur le vrai device du jeu.

typedef IDirect3D8*(WINAPI* Direct3DCreate8_t)(UINT);
typedef HRESULT(WINAPI* CreateDevice_t)(IDirect3D8*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice8**);
typedef HRESULT(WINAPI* Present_t)(IDirect3DDevice8*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(WINAPI* Reset_t)(IDirect3DDevice8*, D3DPRESENT_PARAMETERS*);
static Direct3DCreate8_t oDirect3DCreate8 = nullptr;
static CreateDevice_t oCreateDevice = nullptr;
static Present_t oPresent = nullptr;
static Reset_t oReset = nullptr;

static void DrawFrame(IDirect3DDevice8* dev) {
    static bool first = true;
    if (first) { first = false; Log("Premier Present intercepte (device=%p)", (void*)dev); }
    if (g_cfg.debugRender == 0) return;
    if (!g_imguiReady) InitImGui(dev);

    HandleKeys();

    HRESULT coop = dev->TestCooperativeLevel();
    static HRESULT lastCoop = D3D_OK;
    if (coop != lastCoop) { Log("TestCooperativeLevel -> 0x%08lX", (unsigned long)coop); lastCoop = coop; }
    if (coop == D3D_OK) {
        ImGui_ImplDX8_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawOverlay();
        ImGui::EndFrame();
        ImGui::Render();
        if (g_cfg.debugRender >= 2 && SUCCEEDED(dev->BeginScene())) {
            ImGui_ImplDX8_RenderDrawData(ImGui::GetDrawData());
            dev->EndScene();
        }
    }
}

// Mesures de performance, écrites dans le log toutes les 5 secondes
struct PerfStats {
    LARGE_INTEGER freq = {}, last = {}, windowStart = {};
    double sumFrame = 0, maxFrame = 0, sumMod = 0, maxMod = 0, sumPresent = 0;
    int frames = 0;
};
static PerfStats g_perf;

static double Ms(LARGE_INTEGER a, LARGE_INTEGER b) { return (b.QuadPart - a.QuadPart) * 1000.0 / g_perf.freq.QuadPart; }

static HRESULT WINAPI hkPresent(IDirect3DDevice8* dev, const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty) {
    LARGE_INTEGER t0, t1, t2;
    if (!g_perf.freq.QuadPart) QueryPerformanceFrequency(&g_perf.freq);
    QueryPerformanceCounter(&t0);

    DrawFrame(dev);

    QueryPerformanceCounter(&t1);
    HRESULT hr = oPresent(dev, src, dst, wnd, dirty);
    QueryPerformanceCounter(&t2);

    static HRESULT lastPresent = D3D_OK;
    if (hr != lastPresent) { Log("Present -> 0x%08lX", (unsigned long)hr); lastPresent = hr; }

    PerfStats& p = g_perf;
    if (p.last.QuadPart) {
        double frame = Ms(p.last, t0), mod = Ms(t0, t1);
        p.sumFrame += frame; if (frame > p.maxFrame) p.maxFrame = frame;
        p.sumMod += mod; if (mod > p.maxMod) p.maxMod = mod;
        p.sumPresent += Ms(t1, t2);
        p.frames++;
    } else {
        p.windowStart = t0;
    }
    p.last = t0;
    if (p.frames && Ms(p.windowStart, t0) >= 5000.0) {
        Log("Perf : %.1f img/s, image moy %.1f ms (max %.1f), mod moy %.3f ms (max %.2f), Present moy %.1f ms",
            p.frames * 1000.0 / Ms(p.windowStart, t0), p.sumFrame / p.frames, p.maxFrame,
            p.sumMod / p.frames, p.maxMod, p.sumPresent / p.frames);
        p.sumFrame = p.maxFrame = p.sumMod = p.maxMod = p.sumPresent = 0;
        p.frames = 0;
        p.windowStart = t0;
    }
    return hr;
}

static HRESULT WINAPI hkReset(IDirect3DDevice8* dev, D3DPRESENT_PARAMETERS* pp) {
    if (g_imguiReady) ImGui_ImplDX8_InvalidateDeviceObjects();
    HRESULT coop = dev->TestCooperativeLevel();
    HRESULT hr = oReset(dev, pp);
    Log("Reset (coop avant=0x%08lX) fenetre=%d %ux%u fmt=%d swap=%d -> 0x%08lX", (unsigned long)coop,
        pp->Windowed, pp->BackBufferWidth, pp->BackBufferHeight, (int)pp->BackBufferFormat, (int)pp->SwapEffect,
        (unsigned long)hr);
    return hr;
}

static void HookOnce(void* target, void* detour, void** original, const char* name) {
    if (*original) return;
    MH_STATUS st = MH_CreateHook(target, detour, original);
    if (st == MH_OK) st = MH_EnableHook(target);
    Log("Hook %s @ %p -> %s", name, target, MH_StatusToString(st));
}

static HRESULT WINAPI hkCreateDevice(IDirect3D8* d3d, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                                     D3DPRESENT_PARAMETERS* pp, IDirect3DDevice8** out) {
    HRESULT hr = oCreateDevice(d3d, adapter, type, focus, flags, pp, out);
    Log("CreateDevice du jeu : type=%d flags=0x%08lX fenetre=%d %ux%u -> 0x%08lX", (int)type, flags,
        pp ? pp->Windowed : -1, pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0, (unsigned long)hr);
    if (SUCCEEDED(hr) && out && *out) {
        void** vtbl = *(void***)*out;
        HookOnce(vtbl[15], (void*)&hkPresent, (void**)&oPresent, "Device8::Present");
        HookOnce(vtbl[14], (void*)&hkReset, (void**)&oReset, "Device8::Reset");
    }
    return hr;
}

static IDirect3D8* WINAPI hkDirect3DCreate8(UINT sdk) {
    IDirect3D8* d3d = oDirect3DCreate8(sdk);
    Log("Direct3DCreate8(%u) -> %p", sdk, (void*)d3d);
    if (d3d) HookOnce((*(void***)d3d)[15], (void*)&hkCreateDevice, (void**)&oCreateDevice, "IDirect3D8::CreateDevice");
    return d3d;
}

// Remplace une fonction importée par le module principal (PES6.exe)
static bool PatchIAT(const char* dll, const char* func, void* detour, void** original) {
    BYTE* base = (BYTE*)GetModuleHandleA(nullptr);
    auto dos = (IMAGE_DOS_HEADER*)base;
    auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (auto imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir.VirtualAddress); imp->Name; ++imp) {
        if (lstrcmpiA((char*)(base + imp->Name), dll) != 0) continue;
        auto names = (IMAGE_THUNK_DATA*)(base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto addrs = (IMAGE_THUNK_DATA*)(base + imp->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++addrs) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            auto byName = (IMAGE_IMPORT_BY_NAME*)(base + names->u1.AddressOfData);
            if (lstrcmpA((char*)byName->Name, func) != 0) continue;
            DWORD old;
            VirtualProtect(&addrs->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
            *original = (void*)addrs->u1.Function;
            addrs->u1.Function = (DWORD_PTR)detour;
            VirtualProtect(&addrs->u1.Function, sizeof(void*), old, &old);
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------- DllMain

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameA(nullptr, g_dir, MAX_PATH);
        char* slash = strrchr(g_dir, '\\');
        // settings.exe charge aussi dinput8.dll : hors de PES6.exe on reste un simple relais
        if (!slash || lstrcmpiA(slash + 1, "PES6.exe") != 0) return TRUE;
        *slash = 0;
        wsprintfA(g_ini, "%s\\pes6fun.ini", g_dir);
        char logPath[MAX_PATH];
        wsprintfA(logPath, "%s\\pes6fun.log", g_dir);
        g_log = fopen(logPath, "w");
        LoadConfig();
        Log("PES6 Mode FUN charge (ModeFun=%d)", g_cfg.modeFun);
        if (MH_Initialize() != MH_OK) Log("MH_Initialize a echoue");
        bool ok = PatchIAT("d3d8.dll", "Direct3DCreate8", (void*)&hkDirect3DCreate8, (void**)&oDirect3DCreate8);
        Log("IAT Direct3DCreate8 : %s", ok ? "OK" : "introuvable");
    }
    return TRUE;
}
