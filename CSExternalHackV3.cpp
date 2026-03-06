#include <windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#include <cwchar>

struct Vector3 { float x, y, z; };

DWORD GetProcessId(const wchar_t* processName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0) {
                CloseHandle(snap);
                return pe.th32ProcessID;
            }
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

template<typename T>
T ReadMemory(HANDLE hProcess, uintptr_t address) {
    T value{};
    ReadProcessMemory(hProcess, (LPCVOID)address, &value, sizeof(T), nullptr);
    return value;
}

uintptr_t FindPattern(HANDLE hProcess, BYTE* pattern, const char* mask, uintptr_t start, size_t size) {

    BYTE* buffer = new BYTE[size];
    SIZE_T bytesRead;

    if (!ReadProcessMemory(hProcess, (LPCVOID)start, buffer, size, &bytesRead)) {
        delete[] buffer;
        return 0;
    }

    for (size_t i = 0; i < bytesRead - strlen(mask); i++) {

        bool found = true;

        for (size_t j = 0; j < strlen(mask); j++) {
            if (mask[j] != '?' && buffer[i + j] != pattern[j]) {
                found = false;
                break;
            }
        }

        if (found) {
            uintptr_t result = start + i;
            delete[] buffer;
            return result;
        }
    }

    delete[] buffer;
    return 0;
}

class ExternalCSHack {

private:

    HANDLE hProcess = nullptr;
    HWND hwnd = nullptr;
    HDC hdc = nullptr;

    uintptr_t clientBase = 0;
    uintptr_t dwLocalPlayer = 0;
    uintptr_t dwEntityList = 0;

public:

    bool Init() {

        DWORD pid = GetProcessId(L"hl2.exe");
        if (!pid) return false;

        hProcess = OpenProcess(PROCESS_VM_READ, FALSE, pid);
        if (!hProcess) return false;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);

        MODULEENTRY32W me;
        me.dwSize = sizeof(me);

        if (Module32FirstW(snap, &me)) {

            do {

                if (_wcsicmp(me.szModule, L"client.dll") == 0) {
                    clientBase = (uintptr_t)me.modBaseAddr;
                    break;
                }

            } while (Module32NextW(snap, &me));
        }

        CloseHandle(snap);

        dwLocalPlayer = FindPattern(hProcess, (BYTE*)"\x8B\x0D????\x83\xFF\xFF", "x??xxxxx", clientBase, 0x2000000);
        dwEntityList = clientBase + 0x4A8B2C;

        CreateOverlay();

        return true;
    }

    void CreateOverlay() {

        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CSHackOverlay";

        RegisterClassW(&wc);

        hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED,
            L"CSHackOverlay",
            L"ESP",
            WS_POPUP,
            0,
            0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
            nullptr,
            nullptr,
            wc.hInstance,
            nullptr
        );

        SetLayeredWindowAttributes(hwnd, 0, 220, LWA_ALPHA);

        ShowWindow(hwnd, SW_SHOW);
    }

    void RenderESP() {

        hdc = GetDC(hwnd);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0,255,0));

        uintptr_t localPlayer = ReadMemory<uintptr_t>(hProcess, dwLocalPlayer);
        if (!localPlayer) {
            ReleaseDC(hwnd, hdc);
            return;
        }

        int localTeam = ReadMemory<int>(hProcess, localPlayer + 0xF0);

        for (int i = 1; i < 32; i++) {

            uintptr_t entity = ReadMemory<uintptr_t>(hProcess, dwEntityList + i * 0x10);

            if (!entity || entity == localPlayer)
                continue;

            int team = ReadMemory<int>(hProcess, entity + 0xF0);
            int health = ReadMemory<int>(hProcess, entity + 0x100);

            if (team == localTeam || health < 1)
                continue;

            int screenX = 960 + (i * 30);
            int screenY = 540 + (i * 40);

            RECT box = { screenX - 25, screenY - 50, screenX + 25, screenY + 50 };

            Rectangle(hdc, box.left, box.top, box.right, box.bottom);

            wchar_t buf[64];
            swprintf(buf, 64, L"Enemy %d HP:%d", i, health);

            TextOutW(hdc, box.left, box.top - 20, buf, wcslen(buf));
        }

        ReleaseDC(hwnd, hdc);
    }

    void Triggerbot() {

        uintptr_t localPlayer = ReadMemory<uintptr_t>(hProcess, dwLocalPlayer);
        if (!localPlayer) return;

        int crosshairId = ReadMemory<int>(hProcess, localPlayer + 0xB2AC);

        if (crosshairId > 0 && crosshairId < 64) {

            uintptr_t entity = ReadMemory<uintptr_t>(hProcess, dwEntityList + (crosshairId - 1) * 0x10);

            int enemyTeam = ReadMemory<int>(hProcess, entity + 0xF0);
            int localTeam = ReadMemory<int>(hProcess, localPlayer + 0xF0);
            int health = ReadMemory<int>(hProcess, entity + 0x100);

            if (enemyTeam != localTeam && health > 0 && health <= 100) {

                mouse_event(MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);

            }
        }
    }

    void Run() {

        std::wcout << L"CS:Source External Hack v2.0\n";
        std::wcout << L"F1=ESP | F2=Triggerbot | F3=BunnyHop | ESC=Exit\n\n";

        bool esp = false, trigger = false, bhop = false;

        while (true) {

            if (GetAsyncKeyState(VK_F1) & 1) esp = !esp;
            if (GetAsyncKeyState(VK_F2) & 1) trigger = !trigger;
            if (GetAsyncKeyState(VK_F3) & 1) bhop = !bhop;
            if (GetAsyncKeyState(VK_ESCAPE) & 1) break;

            if (esp) RenderESP();
            if (trigger) Triggerbot();
            if (bhop) BunnyHop();

            Sleep(1);
        }

        CloseHandle(hProcess);
    }

private:

    void BunnyHop() {

        static bool jumping = false;

        if (GetAsyncKeyState(VK_SPACE) & 0x8000) {

            if (!jumping) {

                keybd_event(VK_SPACE, 0, 0, 0);
                keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);

                jumping = true;
            }

        }
        else {

            jumping = false;

        }

        static float lastTime = 0;

        float currentTime = (float)GetTickCount64() / 1000.0f;

        if (currentTime - lastTime > 0.05f) {

            if (GetAsyncKeyState('A') & 0x8000) {
                mouse_event(MOUSEEVENTF_MOVE, -5, 0, 0, 0);
            }
            else if (GetAsyncKeyState('D') & 0x8000) {
                mouse_event(MOUSEEVENTF_MOVE, 5, 0, 0, 0);
            }

            lastTime = currentTime;
        }
    }
};

int main() {

    ExternalCSHack hack;

    if (hack.Init()) {
        hack.Run();
    }
    else {
        std::wcout << L"Failed to attach to hl2.exe\n";
    }

    std::wcout << L"Press ENTER to exit...";
    std::cin.get();

    return 0;
}