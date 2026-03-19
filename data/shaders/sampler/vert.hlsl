struct VertexIn {
    [[vk::location(0)]] float3 pos : POSITION;
    [[vk::location(1)]] float2 texcoord : TEXCOORD;
};

struct VertexOut {
    float4 pos_h : SV_POSITION;
    [[vk::location(0)]] float2 texcoord : TEXCOORD;
};

VertexOut main(in VertexIn input) {
    VertexOut output;
    output.pos_h = float4(input.pos, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}
