struct VS_INPUT {
    float2 pos : POSITION;
    float3 col : COLOR0;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float3 col : COLOR0;
};

PS_INPUT vs(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos.xy, 0.f, 1.f);
    output.col = input.col;
    return output;
}

float4 ps(PS_INPUT input) : SV_Target {
    return float4(input.col, 1.f);
}
