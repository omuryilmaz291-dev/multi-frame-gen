#pragma once
#include "Common.h"
#include <d3d11.h>
#include <dxgi1_2.h>

// Tam ekran, tiklama gecirgen, topmost, yakalamadan haric tutulan D3D11 overlay.
class OverlayWindow {
public:
    bool Create(HINSTANCE hi, ID3D11Device* dev, RECT r);
    HRESULT Present(UINT syncInterval) { return sc_->Present(syncInterval, 0); }
    ID3D11UnorderedAccessView* UAV() const { return uav_.Get(); }
    void KeepOnTop();
    void Show(bool s) { ShowWindow(hwnd_, s ? SW_SHOWNOACTIVATE : SW_HIDE); if (s) KeepOnTop(); }
    HWND Hwnd() const { return hwnd_; }
private:
    HWND hwnd_ = nullptr;
    ComPtr<IDXGISwapChain1> sc_;
    ComPtr<ID3D11UnorderedAccessView> uav_;
};
