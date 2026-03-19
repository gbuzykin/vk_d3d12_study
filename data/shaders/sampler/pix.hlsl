struct FragmentIn {
    float4 pos_h : SV_POSITION;
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

Texture2D tex2D : register(t0);
SamplerState mySampler : register(s0);

float4 main(in FragmentIn input) : SV_TARGET {
    return tex2D.Sample(mySampler, input.texcoord);
}
