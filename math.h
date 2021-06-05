#define m_min(a, b)            ((a) < (b) ? (a) : (b))
#define m_max(a, b)            ((a) > (b) ? (a) : (b))
#define clamp(x, a, b)         m_min(b, m_max(a, x))

#define sat_i8(x) (int8_t) clamp((x), -128, 127)

#define PI_f (3.14159265359f)

const uint32_t positive_inf = 0x7F800000; // 0xFF << 23
#define INFINITY (*(float *)&positive_inf)

static float fabsf(float f) {
    return (f < 0.0f) ? -f : f;
}

static float sign(float f) {
    if (f > 0.0) return -1.0f;
    if (f < 0.0) return  1.0f;
    else         return  0.0f;
}

static float step(float edge, float x) {
    return (x < edge) ? 0.0f : 1.0f;
}

typedef struct { float x, y; } Vec2;
static Vec2 vec2(float x, float y) {
    return (Vec2) { x, y };
}

static Vec2 vec2_rot(float rot) {
    return vec2(cosf(rot), sinf(rot));
}

static float rot_vec2(Vec2 rot) {
    return atan2f(rot.y, rot.x);
}

static Vec2 add2(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x,
                a.y + b.y);
}

static Vec2 sub2(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x,
                a.y - b.y);
}

static Vec2 sub2_f(Vec2 v, float f) {
    return vec2(v.x - f,
                v.y - f);
}

static Vec2 div2(Vec2 a, Vec2 b) {
    return vec2(a.x / b.x,
                a.y / b.y);
}

static Vec2 div2_f(Vec2 a, float f) {
    return vec2(a.x / f,
                a.y / f);
}

static Vec2 mul2(Vec2 a, Vec2 b) {
    return vec2(a.x * b.x,
                a.y * b.y);
}

static Vec2 mul2_f(Vec2 a, float f) {
    return vec2(a.x * f,
                a.y * f);
}

static float dot2(Vec2 a, Vec2 b) {
    return a.x*b.x + a.y*b.y;
}

static float mag2(Vec2 a) {
    return sqrtf(dot2(a, a));
}

static float magmag2(Vec2 a) {
    return dot2(a, a);
}

static Vec2 norm2(Vec2 a) {
    return div2_f(a, mag2(a));
}


typedef struct { float x, y, z; } Vec3;
#define vec3_f(f) ((Vec3) { f, f, f })
#define vec3_x ((Vec3) { 1.0f, 0.0f, 0.0f })
#define vec3_y ((Vec3) { 0.0f, 1.0f, 0.0f })
#define vec3_z ((Vec3) { 0.0f, 0.0f, 1.0f })

static Vec3 vec3(float x, float y, float z) {
    return (Vec3) { x, y, z };
}

static Vec3 add3(Vec3 a, Vec3 b) {
    return vec3(a.x + b.x,
                a.y + b.y,
                a.z + b.z);
}

static Vec3 add3_f(Vec3 a, float f) {
    return vec3(a.x + f,
                a.y + f,
                a.z + f);
}

static Vec3 sub3(Vec3 a, Vec3 b) {
    return vec3(a.x - b.x,
                a.y - b.y,
                a.z - b.z);
}

static Vec3 div3(Vec3 a, Vec3 b) {
    return vec3(a.x / b.x,
                a.y / b.y,
                a.z / b.z);
}

static Vec3 div3_f(Vec3 a, float f) {
    return vec3(a.x / f,
                a.y / f,
                a.z / f);
}

static Vec3 mul3(Vec3 a, Vec3 b) {
    return vec3(a.x * b.x,
                a.y * b.y,
                a.z * b.z);
}

static Vec3 mul3_f(Vec3 a, float f) {
    return vec3(a.x * f,
                a.y * f,
                a.z * f);
}

static Vec3 abs3(Vec3 v) {
    return vec3(fabsf(v.x), fabsf(v.y), fabsf(v.z));
}

static Vec3 sign3(Vec3 v) {
    return vec3(sign(v.x), sign(v.y), sign(v.z));
}

static Vec3 max3_f(Vec3 v, float f) {
    return vec3(m_max(v.x, f), m_max(v.y, f), m_max(v.z, f));
}
static Vec3 yzx3(Vec3 v) { return vec3(v.y, v.z, v.x); }
static Vec3 zxy3(Vec3 v) { return vec3(v.z, v.x, v.y); }
static Vec3 step3(Vec3 a, Vec3 b) {
    return vec3(step(a.x, b.x), step(a.y, b.y), step(a.z, b.z));
}

static Vec3 lerp3(Vec3 a, Vec3 b, float t) {
    return add3(mul3_f(a, 1.0f - t), mul3_f(b, t));
}

static float dot3(Vec3 a, Vec3 b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static Vec3 cross3(Vec3 a, Vec3 b) {
    return vec3((a.y * b.z) - (a.z * b.y),
                (a.z * b.x) - (a.x * b.z),
                (a.x * b.y) - (a.y * b.x));
}

static float mag3(Vec3 a) {
    return sqrtf(dot3(a, a));
}

static float magmag3(Vec3 a) {
    return dot3(a, a);
}

static Vec3 norm3(Vec3 a) {
    return div3_f(a, mag3(a));
}


typedef struct { float x, y, z, w; } Vec4;
typedef union {
    float nums[4][4];
    struct { Vec4 x, y, z, w; };
    Vec4 cols[4];
} Mat4;

static Mat4 mul4x4(Mat4 a, Mat4 b) {
    Mat4 out = {0};
    int8_t k, r, c;
    for (c = 0; c < 4; ++c)
        for (r = 0; r < 4; ++r) {
            out.nums[c][r] = 0.0f;
            for (k = 0; k < 4; ++k)
                out.nums[c][r] += a.nums[k][r] * b.nums[c][k];
        }
    return out;
}

static Mat4 ident4x4() {
    Mat4 res = {0};
    res.nums[0][0] = 1.0f;
    res.nums[1][1] = 1.0f;
    res.nums[2][2] = 1.0f;
    res.nums[3][3] = 1.0f;
    return res;
}

static Mat4 transpose4x4(Mat4 a) {
    Mat4 res;
    for(int c = 0; c < 4; ++c)
        for(int r = 0; r < 4; ++r)
            res.nums[r][c] = a.nums[c][r];
    return res;
}

static Mat4 translate4x4(Vec3 pos) {
    Mat4 res = ident4x4();
    res.nums[3][0] = pos.x;
    res.nums[3][1] = pos.y;
    res.nums[3][2] = pos.z;
    return res;
}

static Mat4 rotate4x4(Vec3 axis, float angle) {
    Mat4 res = ident4x4();

    axis = norm3(axis);

    float sin_theta = sinf(angle);
    float cos_theta = cosf(angle);
    float cos_value = 1.0f - cos_theta;

    res.nums[0][0] = (axis.x * axis.x * cos_value) + cos_theta;
    res.nums[0][1] = (axis.x * axis.y * cos_value) + (axis.z * sin_theta);
    res.nums[0][2] = (axis.x * axis.z * cos_value) - (axis.y * sin_theta);

    res.nums[1][0] = (axis.y * axis.x * cos_value) - (axis.z * sin_theta);
    res.nums[1][1] = (axis.y * axis.y * cos_value) + cos_theta;
    res.nums[1][2] = (axis.y * axis.z * cos_value) + (axis.x * sin_theta);

    res.nums[2][0] = (axis.z * axis.x * cos_value) + (axis.y * sin_theta);
    res.nums[2][1] = (axis.z * axis.y * cos_value) - (axis.x * sin_theta);
    res.nums[2][2] = (axis.z * axis.z * cos_value) + cos_theta;

    return res;
}

/* equivalent to XMMatrixPerspectiveFovLH
   https://docs.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixperspectivefovlh
*/
static Mat4 perspective4x4(float fov, float aspect, float near, float far) {
    fov *= 0.5f;
    float height = cosf(fov) / sinf(fov);
    float width = height / aspect;
    float f_range = far / (far - near);

    Mat4 res = {0};
    res.nums[0][0] = width;
    res.nums[1][1] = height;
    res.nums[2][3] = 1.0f;
    res.nums[2][2] = f_range;
    res.nums[3][2] = -f_range * near;
    return res;
}

static Mat4 look_at4x4(Vec3 eye, Vec3 focus, Vec3 up) {
    Vec3 eye_dir = sub3(focus, eye);
    Vec3 R2 = norm3(eye_dir);

    Vec3 R0 = norm3(cross3(up, R2));
    Vec3 R1 = cross3(R2, R0);

    Vec3 neg_eye = mul3_f(eye, -1.0f);

    float D0 = dot3(R0, neg_eye);
    float D1 = dot3(R1, neg_eye);
    float D2 = dot3(R2, neg_eye);

    return (Mat4) {{
        { R0.x, R1.x, R2.x, 0.0f },
        { R0.y, R1.y, R2.y, 0.0f },
        { R0.z, R1.z, R2.z, 0.0f },
        {   D0,   D1,   D2, 1.0f }
    }};
}
