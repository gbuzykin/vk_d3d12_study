struct VertexIn {
    location(0) float3 pos : POSITION;
};

struct VertexOut {
    float4 pos_h : SV_POSITION;
};

VertexOut main(in VertexIn input) {
    VertexOut output;
    output.pos_h = float4(input.pos, 1.0f);
    return output;
}
