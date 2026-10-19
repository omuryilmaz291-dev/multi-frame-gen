// Pass 1: CSLuma - iki kareyi 1/4 cozunurlukte luma'ya indirger.
// Pass 2: CSGrad - A karesinin luma gradyanini BIR KEZ hesaplar (iterasyonlarda tekrar hesaplanmaz).
// Pass 3: CSFlow - Lucas-Kanade; yapi tensoru BIR KEZ, sonra sadece zamansal terimler iterasyonla guncellenir.
//                  Cikis: UV biriminde hareket vektoru (A -> B). Tum ara kareler bu akisi paylasir.
cbuffer CB : register(b0)
{
    float2 srcSize;  float2 invSrcSize;
    float2 flowSize; float2 invFlowSize;
    float  t;        float3 pad;
};
SamplerState Smp : register(s0);
Texture2D<float4> SrcA : register(t0);
Texture2D<float4> SrcB : register(t1);
Texture2D<float>  LA   : register(t2);
Texture2D<float>  LB   : register(t3);
Texture2D<float2> Grad : register(t4);
RWTexture2D<float>  LumOutA : register(u0);
RWTexture2D<float>  LumOutB : register(u1);
RWTexture2D<float2> GradOut : register(u2);
RWTexture2D<float2> FlowOut : register(u3);

float Luma4(Texture2D<float4> tex, float2 uv)
{
    float2 o = invSrcSize;
    float3 c = tex.SampleLevel(Smp, uv + float2(-o.x, -o.y), 0).rgb
             + tex.SampleLevel(Smp, uv + float2( o.x, -o.y), 0).rgb
             + tex.SampleLevel(Smp, uv + float2(-o.x,  o.y), 0).rgb
             + tex.SampleLevel(Smp, uv + float2( o.x,  o.y), 0).rgb;
    return dot(c * 0.25, float3(0.299, 0.587, 0.114));
}

[numthreads(8, 8, 1)]
void CSLuma(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= (uint)flowSize.x || id.y >= (uint)flowSize.y) return;
    float2 uv = (id.xy + 0.5) * invFlowSize;
    LumOutA[id.xy] = Luma4(SrcA, uv);
    LumOutB[id.xy] = Luma4(SrcB, uv);
}

[numthreads(8, 8, 1)]
void CSGrad(uint3 id : SV_DispatchThreadID)
{
    int2 fs = int2(flowSize);
    if (id.x >= (uint)fs.x || id.y >= (uint)fs.y) return;
    int2 p  = int2(id.xy);
    int2 lo = int2(0, 0);
    int2 hi = fs - int2(1, 1);
    float l = LA.Load(int3(clamp(p + int2(-1, 0), lo, hi), 0));
    float r = LA.Load(int3(clamp(p + int2( 1, 0), lo, hi), 0));
    float u = LA.Load(int3(clamp(p + int2( 0,-1), lo, hi), 0));
    float d = LA.Load(int3(clamp(p + int2( 0, 1), lo, hi), 0));
    GradOut[id.xy] = float2((r - l) * 0.5, (d - u) * 0.5);
}

static const int   R   = 3;      // pencere yaricapi (7x7)
static const int   ITR = 4;      // LK iterasyon sayisi
static const float EPS = 0.002;  // duz bolgelerde akisi 0'a ceken regularizasyon

[numthreads(8, 8, 1)]
void CSFlow(uint3 id : SV_DispatchThreadID)
{
    int2 fs = int2(flowSize);
    if (id.x >= (uint)fs.x || id.y >= (uint)fs.y) return;
    int2 c  = int2(id.xy);
    int2 lo = int2(0, 0);
    int2 hi = fs - int2(1, 1);

    // 1) Yapi tensoru (iterasyondan bagimsiz) - bir kez
    float sxx = EPS, sxy = 0, syy = EPS;
    [loop] for (int j = -R; j <= R; ++j)
    {
        [loop] for (int i = -R; i <= R; ++i)
        {
            float2 g = Grad.Load(int3(clamp(c + int2(i, j), lo, hi), 0));
            sxx += g.x * g.x; sxy += g.x * g.y; syy += g.y * g.y;
        }
    }
    float det = sxx * syy - sxy * sxy;

    // 2) Iterasyonlar: sadece zamansal terim yeniden hesaplanir
    float2 d = float2(0, 0); // hucre birimi
    [loop] for (int it = 0; it < ITR; ++it)
    {
        float sxt = 0, syt = 0;
        [loop] for (int jj = -R; jj <= R; ++jj)
        {
            [loop] for (int ii = -R; ii <= R; ++ii)
            {
                int2   q  = clamp(c + int2(ii, jj), lo, hi);
                float2 g  = Grad.Load(int3(q, 0));
                float  la = LA.Load(int3(q, 0));
                float  lb = LB.SampleLevel(Smp, (float2(q) + 0.5 + d) * invFlowSize, 0);
                float  tt = lb - la;
                sxt += g.x * tt; syt += g.y * tt;
            }
        }
        float2 dd = float2(-(syy * sxt - sxy * syt), -(sxx * syt - sxy * sxt)) / det;
        d = clamp(d + clamp(dd, float2(-2, -2), float2(2, 2)), float2(-8, -8), float2(8, 8));
    }
    FlowOut[id.xy] = d * invFlowSize;
}
