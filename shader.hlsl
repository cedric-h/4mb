cbuffer UniformBuffer {
    matrix view_proj;
};
struct VS_INPUT {
    float3 pos  : POSITION;
    float3 norm : NORMAL;
};

struct PS_INPUT {
    float4 pos  : SV_Position;
    float3 norm : NORMAL;
};

PS_INPUT vs(VS_INPUT input) {
    PS_INPUT output;
    output.pos = mul(float4(input.pos, 1.0f), view_proj);
    output.norm = input.norm;
    return output;
}

float4 ps(PS_INPUT input) : SV_Target {
    float3 light_dir = normalize(float3(6.0f,18.0f,24.0f));
    float3 light_color = { 1.0f, 0.912f, 0.802f };
    float light_strength = 1.4f;

    float3 diffuse = max(dot(input.norm, light_dir), 0.0f) * light_color;
    float3 ambient = light_color * 0.3f;
    return float4((ambient + diffuse) * light_strength, 1.0f);
}
