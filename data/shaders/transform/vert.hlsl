struct VertexIn {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float3 normal : NORMAL;
    [[vk::location(2)]] float2 texcoord : TEXCOORD;
};

struct CB0 {
    float4x4 mvp;
    float3x3 mv;
};

[[vk::binding(1)]] ConstantBuffer<CB0> cb0 : register(b0);

struct VertexOut {
    float4 pos_h : SV_POSITION;
    [[vk::location(0)]] float4 color : COLOR;
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
};

VertexOut main(in VertexIn input) {
    VertexOut output;
    float3 normal = mul(cb0.mv, input.normal);
    output.pos_h = mul(cb0.mvp, float4(input.pos, 1.0));
    output.color = max(0.0, dot(normal, float3(0.58, 0.58, 0.58))) + 0.1;
    output.texcoord = input.texcoord;
    return output;
}
