#include "FrameInterpolator.h"
#include "Shaders.h"   // CMake tarafindan shaders/*.hlsl'den uretilir (exe'ye gomulu)
#include <d3dcompiler.h>
#include <cstring>

static ComPtr<ID3D11ComputeShader> Load(ID3D11Device* d, const char* src, const char* name, const char* entry) {
    ComPtr<ID3DBlob> b, e;
    HRESULT hr = D3DCompile(src, strlen(src), name, nullptr, nullptr, entry, "cs_5_0",
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &b, &e);
    if (FAILED(hr)) {
        Fail(e ? (const char*)e->GetBufferPointer() : "Shader derlenemedi.");
        return nullptr;
    }
    ComPtr<ID3D11ComputeShader> cs;
    if (FAILED(d->CreateComputeShader(b->GetBufferPointer(), b->GetBufferSize(), nullptr, &cs))) {
        Fail("Compute shader olusturulamadi (DirectX 11 compute destekli GPU gerekli).");
        return nullptr;
    }
    return cs;
}

bool FrameInterpolator::MakeTex(UINT w, UINT h, DXGI_FORMAT f, ComPtr<ID3D11ShaderResourceView>& s, ComPtr<ID3D11UnorderedAccessView>& u) {
    D3D11_TEXTURE2D_DESC d{};
    d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = f; d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> t;
    return SUCCEEDED(dev_->CreateTexture2D(&d, nullptr, &t)) &&
           SUCCEEDED(dev_->CreateShaderResourceView(t.Get(), nullptr, &s)) &&
           SUCCEEDED(dev_->CreateUnorderedAccessView(t.Get(), nullptr, &u));
}

bool FrameInterpolator::Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, UINT w, UINT h) {
    dev_ = dev; ctx_ = ctx; w_ = w; h_ = h; fw_ = (w + 3) / 4; fh_ = (h + 3) / 4;
    csLuma_ = Load(dev, Shaders::OpticalFlow,  "OpticalFlow.hlsl",  "CSLuma");
    csGrad_ = Load(dev, Shaders::OpticalFlow,  "OpticalFlow.hlsl",  "CSGrad");
    csFlow_ = Load(dev, Shaders::OpticalFlow,  "OpticalFlow.hlsl",  "CSFlow");
    csWarp_ = Load(dev, Shaders::FrameWarping, "FrameWarping.hlsl", "CSWarp");
    csBlit_ = Load(dev, Shaders::FrameWarping, "FrameWarping.hlsl", "CSBlit");
    if (!csLuma_ || !csGrad_ || !csFlow_ || !csWarp_ || !csBlit_) return false;
    if (!MakeTex(fw_, fh_, DXGI_FORMAT_R32_FLOAT,       lumASrv_, lumAUav_) ||
        !MakeTex(fw_, fh_, DXGI_FORMAT_R32_FLOAT,       lumBSrv_, lumBUav_) ||
        !MakeTex(fw_, fh_, DXGI_FORMAT_R32G32_FLOAT,    gradSrv_, gradUav_) ||
        !MakeTex(fw_, fh_, DXGI_FORMAT_R32G32_FLOAT,    flowSrv_, flowUav_)) { Fail("Flow dokulari olusturulamadi."); return false; }
    D3D11_BUFFER_DESC bd{}; bd.ByteWidth = 48; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &cb_)) || FAILED(dev->CreateSamplerState(&sd, &smp_))) return false;
    return true;
}

void FrameInterpolator::UpdateCB(float t) {
    float v[12] = { (float)w_, (float)h_, 1.f / w_, 1.f / h_, (float)fw_, (float)fh_, 1.f / fw_, 1.f / fh_, t, 0, 0, 0 };
    ctx_->UpdateSubresource(cb_.Get(), 0, nullptr, v, 0, 0);
    ctx_->CSSetConstantBuffers(0, 1, cb_.GetAddressOf());
    ctx_->CSSetSamplers(0, 1, smp_.GetAddressOf());
}

void FrameInterpolator::Unbind() {
    ID3D11ShaderResourceView* ns[5] = {};
    ID3D11UnorderedAccessView* nu[4] = {};
    ctx_->CSSetShaderResources(0, 5, ns);
    ctx_->CSSetUnorderedAccessViews(0, 4, nu, nullptr);
}

void FrameInterpolator::ComputeFlow(ID3D11ShaderResourceView* a, ID3D11ShaderResourceView* b) {
    UpdateCB(0.f);
    UINT gx = (fw_ + 7) / 8, gy = (fh_ + 7) / 8;
    // 1) Luma (1/4 cozunurluk): t0,t1 -> u0,u1
    ID3D11ShaderResourceView* s01[2] = { a, b };
    ctx_->CSSetShaderResources(0, 2, s01);
    ID3D11UnorderedAccessView* u01[2] = { lumAUav_.Get(), lumBUav_.Get() };
    ctx_->CSSetUnorderedAccessViews(0, 2, u01, nullptr);
    ctx_->CSSetShader(csLuma_.Get(), nullptr, 0);
    ctx_->Dispatch(gx, gy, 1);
    Unbind();
    // 2) Gradyan: t2 (lumA) -> u2
    ID3D11ShaderResourceView* s2 = lumASrv_.Get();
    ctx_->CSSetShaderResources(2, 1, &s2);
    ID3D11UnorderedAccessView* u2 = gradUav_.Get();
    ctx_->CSSetUnorderedAccessViews(2, 1, &u2, nullptr);
    ctx_->CSSetShader(csGrad_.Get(), nullptr, 0);
    ctx_->Dispatch(gx, gy, 1);
    Unbind();
    // 3) Lucas-Kanade: t2 (lumA), t3 (lumB), t4 (grad) -> u3 (flow)
    ID3D11ShaderResourceView* s234[3] = { lumASrv_.Get(), lumBSrv_.Get(), gradSrv_.Get() };
    ctx_->CSSetShaderResources(2, 3, s234);
    ID3D11UnorderedAccessView* u3 = flowUav_.Get();
    ctx_->CSSetUnorderedAccessViews(3, 1, &u3, nullptr);
    ctx_->CSSetShader(csFlow_.Get(), nullptr, 0);
    ctx_->Dispatch(gx, gy, 1);
    Unbind();
}

void FrameInterpolator::Warp(ID3D11ShaderResourceView* a, ID3D11ShaderResourceView* b, ID3D11UnorderedAccessView* out, float t) {
    UpdateCB(t);
    ID3D11ShaderResourceView* s012[3] = { a, b, flowSrv_.Get() };
    ctx_->CSSetShaderResources(0, 3, s012);
    ctx_->CSSetUnorderedAccessViews(0, 1, &out, nullptr);
    ctx_->CSSetShader(csWarp_.Get(), nullptr, 0);
    ctx_->Dispatch((w_ + 7) / 8, (h_ + 7) / 8, 1);
    Unbind();
}

void FrameInterpolator::Blit(ID3D11ShaderResourceView* src, ID3D11UnorderedAccessView* out) {
    UpdateCB(0.f);
    ctx_->CSSetShaderResources(0, 1, &src);
    ctx_->CSSetUnorderedAccessViews(0, 1, &out, nullptr);
    ctx_->CSSetShader(csBlit_.Get(), nullptr, 0);
    ctx_->Dispatch((w_ + 7) / 8, (h_ + 7) / 8, 1);
    Unbind();
}
