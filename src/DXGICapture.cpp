#include "DXGICapture.h"

bool DXGICapture::Init(int monitor) {
    ComPtr<IDXGIFactory1> fac;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&fac)))) { Fail("DXGI factory olusturulamadi."); return false; }
    ComPtr<IDXGIAdapter1> ad; ComPtr<IDXGIOutput> out;
    int count = 0; bool found = false;
    for (UINT i = 0; !found && fac->EnumAdapters1(i, ad.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
        for (UINT j = 0; ad->EnumOutputs(j, out.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++j) {
            if (count++ == monitor) { found = true; break; }
        }
    }
    if (!found) { Fail("Belirtilen monitor bulunamadi."); return false; }

    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(ad.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                   &fl, 1, D3D11_SDK_VERSION, &dev_, nullptr, &ctx_);
    if (FAILED(hr)) { Fail("D3D11 cihazi olusturulamadi (DirectX 11 destekli GPU gerekli)."); return false; }
    ComPtr<ID3D11Multithread> mt;
    if (SUCCEEDED(ctx_.As(&mt))) mt->SetMultithreadProtected(TRUE);

    DXGI_OUTPUT_DESC od{}; out->GetDesc(&od);
    rect_ = od.DesktopCoordinates;
    if (FAILED(out.As(&out1_))) { Fail("IDXGIOutput1 desteklenmiyor."); return false; }

    D3D11_TEXTURE2D_DESC d{};
    d.Width = Width(); d.Height = Height(); d.MipLevels = 1; d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM; d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    for (int i = 0; i < kSlots; ++i) {
        if (FAILED(dev_->CreateTexture2D(&d, nullptr, &slots_[i])) ||
            FAILED(dev_->CreateShaderResourceView(slots_[i].Get(), nullptr, &srv_[i]))) {
            Fail("Yakalama dokulari olusturulamadi."); return false;
        }
    }
    return CreateDup();
}

bool DXGICapture::CreateDup() {
    dupl_.Reset();
    return SUCCEEDED(out1_->DuplicateOutput(dev_.Get(), &dupl_));
}

void DXGICapture::Start() { run_ = true; th_ = std::thread(&DXGICapture::Loop, this); }
void DXGICapture::Stop() { run_ = false; cv_.notify_all(); if (th_.joinable()) th_.join(); }

int DXGICapture::PickFree() {
    for (int i = 0; i < kSlots; ++i)
        if (i != heldA_ && i != heldB_ && i != latest_) return i;
    return 0; // ulasilamaz
}

void DXGICapture::Loop() {
    while (run_) {
        if (!dupl_ && !CreateDup()) { Sleep(200); continue; }
        DXGI_OUTDUPL_FRAME_INFO fi{}; ComPtr<IDXGIResource> res;
        HRESULT hr = dupl_->AcquireNextFrame(100, &fi, &res);
        if (hr == DXGI_ERROR_WAIT_TIMEOUT) continue;
        if (FAILED(hr)) { dupl_.Reset(); Sleep(100); continue; } // ACCESS_LOST vb. -> yeniden olustur
        if (fi.LastPresentTime.QuadPart != 0) { // sadece fare hareketi degil, gercek yeni goruntu
            ComPtr<ID3D11Texture2D> tex;
            if (SUCCEEDED(res.As(&tex))) {
                int s; { std::lock_guard<std::mutex> l(mu_); s = PickFree(); }
                ctx_->CopyResource(slots_[s].Get(), tex.Get()); // VRAM -> VRAM, CPU'ya dokunmaz
                { std::lock_guard<std::mutex> l(mu_); latest_ = s; ++frameId_; }
                cv_.notify_one();
            }
        }
        res.Reset();
        dupl_->ReleaseFrame();
    }
}

bool DXGICapture::WaitFrame(uint64_t& lastId, int timeoutMs, int& a, int& b) {
    std::unique_lock<std::mutex> l(mu_);
    if (!cv_.wait_for(l, std::chrono::milliseconds(timeoutMs), [&] { return frameId_ != lastId || !run_; })) return false;
    if (frameId_ == lastId) return false;
    lastId = frameId_;
    heldA_ = heldB_; heldB_ = latest_;
    a = heldA_; b = heldB_;
    return true;
}
