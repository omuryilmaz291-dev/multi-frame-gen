// CSWarp: hareket vektorlerine gore A ve B'yi ara zamana kaydirip harmanlar.
//         Tutarsiz (occlusion / hatali akis) pikseller icin duz harmanlamaya duser.
// CSBlit: gercek kareyi swap chain'e kopyalar.
cbuffer CB : register(b0)
{
    float2 srcSize;  float2 invSrcSize;
    float2 flowSize; float2 invFlowSize;
    float  t;        float3 pad;
};
SamplerState Smp : register(s0);
Texture2D<float4>  A    : register(t0);
Texture2D<float4>  B    : register(t1);
Texture2D<float2>  Flow : register(t2);
RWTexture2D<float4> Out : register(u0);

[numthreads(8, 8, 1)]
void CSWarp(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= (uint)srcSize.x || id.y >= (uint)srcSize.y) return;
    float2 uv = (id.xy + 0.5) * invSrcSize;
    float2 f  = Flow.SampleLevel(Smp, uv, 0);
    float3 ca = A.SampleLevel(Smp, uv - f * t, 0).rgb;
    float3 cb = B.SampleLevel(Smp, uv + f * (1.0 - t), 0).rgb;
    float3 warped   = lerp(ca, cb, t);
    float3 fallback = lerp(A.SampleLevel(Smp, uv, 0).rgb, B.SampleLevel(Smp, uv, 0).rgb, t);
    float  err = dot(abs(ca - cb), float3(0.3333, 0.3333, 0.3333));
    float  k   = smoothstep(0.08, 0.25, err);
    Out[id.xy] = float4(lerp(warped, fallback, k), 1.0);
}

[numthreads(8, 8, 1)]
void CSBlit(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= (uint)srcSize.x || id.y >= (uint)srcSize.y) return;
    Out[id.xy] = float4(A.Load(int3(id.xy, 0)).rgb, 1.0);
}
