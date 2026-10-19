#pragma once
#include "Common.h"
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <cstdint>

// DXGI Desktop Duplication ile ekrani dogrudan VRAM'de (GPU->GPU CopyResource) yakalar.
class DXGICapture {
public:
    static constexpr int kSlots = 4;
    ~DXGICapture() { Stop(); }
    bool Init(int monitorIndex);
    void Start();
    void Stop();
    // Yeni kare gelene kadar bekler. a = onceki kare, b = en yeni kare (slot indeksleri).
    bool WaitFrame(uint64_t& lastId, int timeoutMs, int& a, int& b);

    ID3D11Device* Device() const { return dev_.Get(); }
    ID3D11DeviceContext* Context() const { return ctx_.Get(); }
    ID3D11ShaderResourceView* SRV(int i) const { return srv_[i].Get(); }
    RECT Rect() const { return rect_; }
    UINT Width() const { return rect_.right - rect_.left; }
    UINT Height() const { return rect_.bottom - rect_.top; }

private:
    void Loop();
    bool CreateDup();
    int PickFree();

    ComPtr<ID3D11Device> dev_;
    ComPtr<ID3D11DeviceContext> ctx_;
    ComPtr<IDXGIOutput1> out1_;
    ComPtr<IDXGIOutputDuplication> dupl_;
    ComPtr<ID3D11Texture2D> slots_[kSlots];
    ComPtr<ID3D11ShaderResourceView> srv_[kSlots];
    RECT rect_{};

    std::thread th_;
    std::mutex mu_;
    std::condition_variable cv_;
    std::atomic<bool> run_{false};
    int latest_ = -1, heldA_ = -1, heldB_ = -1;
    uint64_t frameId_ = 0;
};
