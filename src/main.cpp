#include "DXGICapture.h"
#include "FrameInterpolator.h"
#include "OverlayWindow.h"
#include <shellapi.h>
#include <cstdlib>
#include <cwchar>

// Kisayollar: Ctrl+Alt+Q = cikis, Ctrl+Alt+F = ac/kapat, Ctrl+Alt+M = carpan (x2 -> x3 -> x4)
// Sistem tepsisi simgesi: sag/sol tikla -> menu.
namespace {
constexpr UINT WM_TRAY = WM_APP + 1;
enum : UINT { ID_TOGGLE = 100, ID_X2 = 102, ID_X3 = 103, ID_X4 = 104, ID_EXIT = 199 };
constexpr int kMinMult = 2, kMaxMult = 4;

struct State { bool enabled = true; int mult = 2; bool quit = false; bool changed = false; } g;
NOTIFYICONDATAW g_nid{};

void UpdateTray(const wchar_t* balloon = nullptr) {
    const wchar_t* mode = !g.enabled ? L"kapali" : (g.mult == 2 ? L"x2" : g.mult == 3 ? L"x3" : L"x4");
    wsprintfW(g_nid.szTip, L"FrameInterp - %s", mode);
    g_nid.uFlags = NIF_TIP;
    if (balloon) {
        g_nid.uFlags |= NIF_INFO;
        lstrcpynW(g_nid.szInfoTitle, L"FrameInterp", 64);
        lstrcpynW(g_nid.szInfo, balloon, 256);
        g_nid.dwInfoFlags = NIIF_INFO;
    }
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

void SetEnabled(bool e) { g.enabled = e; g.changed = true; UpdateTray(); }
void SetMult(int m)     { g.mult = m; UpdateTray(); }
void CycleMult()        { SetMult(g.mult >= kMaxMult ? kMinMult : g.mult + 1); }

void ShowMenu(HWND h) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g.enabled ? MF_CHECKED : 0), ID_TOGGLE, L"Kare artırma açık (Ctrl+Alt+F)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING | (g.mult == 2 ? MF_CHECKED : 0), ID_X2, L"x2");
    AppendMenuW(m, MF_STRING | (g.mult == 3 ? MF_CHECKED : 0), ID_X3, L"x3");
    AppendMenuW(m, MF_STRING | (g.mult == 4 ? MF_CHECKED : 0), ID_X4, L"x4");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_EXIT, L"Çıkış (Ctrl+Alt+Q)");
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(h); // menunun disari tiklayinca kapanmasi icin gerekli
    TrackPopupMenu(m, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, h, nullptr);
    PostMessageW(h, WM_NULL, 0, 0);
    DestroyMenu(m);
}

LRESULT CALLBACK TrayProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_TRAY) {
        if (l == WM_RBUTTONUP || l == WM_LBUTTONUP) ShowMenu(h);
        return 0;
    }
    if (m == WM_COMMAND) {
        switch (LOWORD(w)) {
        case ID_TOGGLE: SetEnabled(!g.enabled); break;
        case ID_X2: SetMult(2); break;
        case ID_X3: SetMult(3); break;
        case ID_X4: SetMult(4); break;
        case ID_EXIT: g.quit = true; break;
        }
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int) {
    SetProcessDPIAware();
    HANDLE once = CreateMutexW(nullptr, TRUE, L"FrameInterp_SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        Fail("FrameInterp zaten calisiyor. Sag alttaki sistem tepsisindeki simgeden cikabilirsiniz (Ctrl+Alt+Q).");
        return 1;
    }

    int monitor = 0, argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i + 1 < argc; ++i) {
        if (wcscmp(argv[i], L"--monitor") == 0) monitor = _wtoi(argv[i + 1]);
        if (wcscmp(argv[i], L"--mult") == 0) {
            int v = _wtoi(argv[i + 1]);
            g.mult = v < kMinMult ? kMinMult : (v > kMaxMult ? kMaxMult : v);
        }
    }
    LocalFree(argv);

    DXGICapture cap;
    if (!cap.Init(monitor)) return 1;
    OverlayWindow overlay;
    if (!overlay.Create(hi, cap.Device(), cap.Rect())) return 1;
    FrameInterpolator interp;
    if (!interp.Init(cap.Device(), cap.Context(), cap.Width(), cap.Height())) return 1;

    // Gizli pencere: tepsi simgesi mesajlari icin
    WNDCLASSW wc{}; wc.lpfnWndProc = TrayProc; wc.hInstance = hi; wc.lpszClassName = L"FrameInterpTray";
    RegisterClassW(&wc);
    HWND trayWnd = CreateWindowExW(0, wc.lpszClassName, L"FrameInterp", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hi, nullptr);
    g_nid.cbSize = sizeof(g_nid); g_nid.hWnd = trayWnd; g_nid.uID = 1;
    g_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g_nid.szTip, L"FrameInterp", 128);
    Shell_NotifyIconW(NIM_ADD, &g_nid);
    UpdateTray(L"Çalışıyor. Ctrl+Alt+M: x2/x3/x4 · Ctrl+Alt+F: aç/kapat · Ctrl+Alt+Q: çıkış");

    RegisterHotKey(nullptr, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
    RegisterHotKey(nullptr, 2, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'F');
    RegisterHotKey(nullptr, 3, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M');
    cap.Start();

    uint64_t last = 0; int a = -1, b = -1, tick = 0;
    bool running = true, fresh = true;
    while (running) {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
            if (m.message == WM_QUIT) running = false;
            else if (m.message == WM_HOTKEY) {
                if (m.wParam == 1) g.quit = true;
                else if (m.wParam == 2) SetEnabled(!g.enabled);
                else if (m.wParam == 3) CycleMult();
            } else { TranslateMessage(&m); DispatchMessageW(&m); }
        }
        if (g.quit) running = false;
        if (!running) break;

        if (g.changed) { // ac/kapat: kapaliyken overlay gizlenir, oyun dogrudan gorunur
            g.changed = false;
            overlay.Show(g.enabled);
            fresh = true;
        }
        if (!g.enabled) { Sleep(50); continue; }

        if (!cap.WaitFrame(last, 16, a, b)) continue;
        if (++tick % 120 == 0) overlay.KeepOnTop();

        bool ok = true;
        if (!fresh && a >= 0) {
            // Akis kaynak kare basina bir kez; N-1 ara karenin hepsi ayni akisi paylasir.
            const int n = g.mult;
            interp.ComputeFlow(cap.SRV(a), cap.SRV(b));
            for (int k = 1; k < n && ok; ++k) {
                interp.Warp(cap.SRV(a), cap.SRV(b), overlay.UAV(), (float)k / (float)n);
                ok = SUCCEEDED(overlay.Present(1));
            }
        }
        fresh = false;
        if (!ok) break;
        interp.Blit(cap.SRV(b), overlay.UAV()); // gercek kare
        if (FAILED(overlay.Present(1))) break;
    }
    cap.Stop();
    Shell_NotifyIconW(NIM_DELETE, &g_nid);
    if (once) CloseHandle(once);
    return 0;
}
