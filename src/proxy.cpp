// Proxy dinput8.dll : PES6.exe importe seulement DirectInput8Create.
// On charge le vrai dinput8.dll de Windows et on lui transmet l'appel.
#include <windows.h>

typedef HRESULT(WINAPI* DirectInput8Create_t)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

static DirectInput8Create_t RealDirectInput8Create() {
    static DirectInput8Create_t fn = nullptr;
    if (!fn) {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);  // SysWOW64 pour un process 32 bits
        lstrcatA(path, "\\dinput8.dll");
        HMODULE real = LoadLibraryA(path);
        if (real) fn = (DirectInput8Create_t)GetProcAddress(real, "DirectInput8Create");
    }
    return fn;
}

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, REFIID riid, LPVOID* out, LPUNKNOWN outer) {
    DirectInput8Create_t fn = RealDirectInput8Create();
    if (!fn) return E_FAIL;
    return fn(inst, ver, riid, out, outer);
}
