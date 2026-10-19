#pragma once
#include "Common.h"
#include <d3d11.h>

// Luma -> gradyan -> Lucas-Kanade optical flow -> motion-compensated warp. Hepsi GPU'da.
// Coklu kare icin: ComputeFlow() kaynak kare cifti basina BIR kez, Warp() ise her ara kare icin cagrilir.
class FrameInterpolator {
public:
    bool Init(ID3D11Device* dev, ID3D11DeviceContext* ctx, UINT w, UINT h);
    // A -> B hareket akisini hesaplar (1/4 cozunurluk). Sonraki Warp() cagrilari bunu kullanir.
    void ComputeFlow(ID3D11ShaderResourceView* a, ID3D11ShaderResourceView* b);
    // A ile B arasindaki t (0..1) anindaki kareyi uav'ye yazar (once ComputeFlow cagrilmis olmali).
    void Warp(ID3D11ShaderResourceView* a, ID3D11ShaderResourceView* b, ID3D11UnorderedAccessView* out, float t);
    // Gercek kareyi oldugu gibi uav'ye kopyalar.
    void Blit(ID3D11ShaderResourceView* src, ID3D11UnorderedAccessView* out);
private:
    bool MakeTex(UINT w, UINT h, DXGI_FORMAT f, ComPtr<ID3D11ShaderResourceView>& s, ComPtr<ID3D11UnorderedAccessView>& u);
    void Unbind();
    void UpdateCB(float t);

    ID3D11Device* dev_ = nullptr; ID3D11DeviceContext* ctx_ = nullptr;
    UINT w_ = 0, h_ = 0, fw_ = 0, fh_ = 0;
    ComPtr<ID3D11ComputeShader> csLuma_, csGrad_, csFlow_, csWarp_, csBlit_;
    ComPtr<ID3D11Buffer> cb_;
    ComPtr<ID3D11SamplerState> smp_;
    ComPtr<ID3D11ShaderResourceView> lumASrv_, lumBSrv_, gradSrv_, flowSrv_;
    ComPtr<ID3D11UnorderedAccessView> lumAUav_, lumBUav_, gradUav_, flowUav_;
};
