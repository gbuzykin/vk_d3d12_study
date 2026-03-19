struct FragmentIn {
    float4 pos_h : SV_POSITION;
};

float4 main(in FragmentIn input) : SV_TARGET {
    return float4(0.8, 0.4, 0.0, 1.0);
}
