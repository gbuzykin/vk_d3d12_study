struct VertexIn {
    location(0) float3 pos : POSITION;
    location(1) float3 normal : NORMAL;
    location(2) float2 texcoord : TEXCOORD;
};

struct CB0 {
    row_major float4x4 mvp;
    row_major float3x3 mv;
};

binding(1) ConstantBuffer<CB0> cb0 : register(b0);

struct VertexOut {
    float4 pos_h : SV_POSITION;
    location(0) float4 color : COLOR;
    location(1) float2 texcoord : TEXCOORD;
};

VertexOut main(in VertexIn input) {
    VertexOut output;
    float3 normal = mul(input.normal, cb0.mv);
    output.pos_h = mul(float4(input.pos, 1.0), cb0.mvp);
    output.color = max(0.0, dot(normal, float3(0.58, 0.58, 0.58))) + 0.1;
    output.texcoord = input.texcoord;
    return output;
}
