struct VS_INPUT {
    float2 pos    : POSITION;
    float4 col    : COLOR0;
    float2 size   : SIZE;
    float  radius : RADIUS;
    uint   v_id   : SV_VertexID;
};

struct PS_INPUT {
    float4 pos    : SV_Position;
    float4 col    : COLOR0;
    float2 size   : SIZE;
    float2 frame  : FRAME;
    float  radius : RADIUS;
};

PS_INPUT vs(VS_INPUT input) {
    PS_INPUT output;
    output.pos = float4(input.pos.xy, 0.f, 1.0f);
    output.col = input.col;
    output.size = input.size * 65535.0;

    float2 square[4] = {
        float2(-0.5f,  0.5f),
        float2( 0.5f,  0.5f),
        float2(-0.5f, -0.5f),
        float2( 0.5f, -0.5f),
    };
    output.frame = square[input.v_id % 4] * output.size;

    output.radius = max(output.size.x, output.size.y) * input.radius * 0.5f;
    return output;
}

float inv_lerp(float a, float b, float v) {
    return min(1.0f, max(0.0f, (v - a) / (b - a)));
}

float4 ps(PS_INPUT input) : SV_Target {
    float4 output = input.col;

    float2 q = abs(input.frame) - input.size * 0.5f + input.radius;
    float dist = min(max(q.x, q.y), 0.0f) + length(max(q, 0.0f)) - input.radius;
    output *= inv_lerp(0.0f, -2.0f, dist);

    return output;
}
