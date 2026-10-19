#include "OverlayWindow.h"
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_NCHITTEST: return HTTRANSPARENT;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

bool OverlayWindow::Create(HINSTANCE hi, ID3D11Device* dev, RECT r) {
    WNDCLASSW wc{}; wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"FrameInterpOverlay";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    int w = r.right - r.left, h = r.bottom - r.top;
    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                            wc.lpszClassName, L"FrameInterp Overlay", WS_POPUP, r.left, r.top, w, h,
                            nullptr, nullptr, hi, nullptr);
    if (!hwnd_) { Fail("Overlay penceresi olusturulamadi."); return false; }
    SetLayeredWindowAttributes(hwnd_, 0, 255, LWA_ALPHA);
    // Overlay'in kendi kendini yakalamasini (geri besleme dongusu) engeller. Windows 10 2004+ gerekir.
    if (!SetWindowDisplayAffinity(hwnd_, WDA_EXCLUDEFROMCAPTURE)) {
        Fail("SetWindowDisplayAffinity basarisiz. Windows 10 (2004 / build 19041) veya ustu gerekli.");
        return false;
    }
    ComPtr<IDXGIDevice> dd; ComPtr<IDXGIAdapter> ad; ComPtr<IDXGIFactory2> fac;
    dev->QueryInterface(IID_PPV_ARGS(&dd)); dd->GetAdapter(&ad); ad->GetParent(IID_PPV_ARGS(&fac));
    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = w; sd.Height = h; sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
    sd.BufferCount = 2; sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    if (FAILED(fac->CreateSwapChainForHwnd(dev, hwnd_, &sd, nullptr, nullptr, &sc_))) {
        Fail("Swap chain olusturulamadi."); return false;
    }
    fac->MakeWindowAssociation(hwnd_, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    { ComPtr<IDXGIDevice1> d1; if (SUCCEEDED(dev->QueryInterface(IID_PPV_ARGS(&d1)))) d1->SetMaximumFrameLatency(1); } // gecikmeyi dusur
    ComPtr<ID3D11Texture2D> bb;
    sc_->GetBuffer(0, IID_PPV_ARGS(&bb));
    if (FAILED(dev->CreateUnorderedAccessView(bb.Get(), nullptr, &uav_))) {
        Fail("Back buffer UAV olusturulamadi."); return false;
    }
    ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    KeepOnTop();
    return true;
}

void OverlayWindow::KeepOnTop() {
    SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
