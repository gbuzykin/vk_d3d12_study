struct FragmentIn {
    float4 pos_h : SV_POSITION;
    location(0) float4 color : COLOR;
    location(1) float2 texcoord : TEXCOORD;
};

binding(0) Texture2D tex2D : register(t0);
binding(0) SamplerState mySampler : register(s0);

float4 main(in FragmentIn input) : SV_TARGET {
    return input.color * tex2D.Sample(mySampler, input.texcoord);
}
