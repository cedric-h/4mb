#define PI_f  3.14159265359f

typedef union {
    struct { float x, y; };
    float nums[2];
} Vec2;

static Vec2 div2(Vec2 a, Vec2 b)    { return (Vec2){a.x/b.x, a.y/b.y}; }
static Vec2 div2_f(Vec2 v, float f) { return (Vec2){v.x/f  , v.y/f  }; }

static Vec2 sub2(Vec2 a, Vec2 b)    { return (Vec2){a.x-b.x, a.y-b.y}; }
static Vec2 sub2_f(Vec2 a, float f) { return (Vec2){a.x-f  , a.y-f  }; }

static Vec2 add2(Vec2 a, Vec2 b)    { return (Vec2){a.x+b.x, a.y+b.y}; }
static Vec2 add2_f(Vec2 a, float f) { return (Vec2){a.x+f  , a.y+f  }; }

static Vec2 mul2(Vec2 a, Vec2 b)    { return (Vec2){a.x*b.x, a.y*b.y}; }
static Vec2 mul2_f(Vec2 a, float f) { return (Vec2){a.x*f  , a.y*f  }; }

static Vec2 vec2_rot(float rot) { return (Vec2){cosf(rot), sinf(rot)}; }
static float rot_vec2(Vec2 rot) { return atan2f(rot.y, rot.x); }

static float dot2(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }
static float magmag2(Vec2 v) { return dot2(v, v); }
static float mag2(Vec2 v) { return sqrtf(magmag2(v)); }

typedef union {
    struct { uint8_t x, y, z; };
    struct { uint8_t r, g, b; };
    uint8_t nums[3];
} Byte3;
typedef union {
    struct { uint8_t x, y, z, w; };
    struct { uint8_t r, g, b, a; };
    uint8_t nums[4];
} Byte4;
static Byte4 byte4_u(uint8_t u) { return (Byte4){u, u, u, u}; }

typedef union {
    struct { float x, y, z; };
    struct { float r, g, b; };
    Vec2 xy;
    float nums[3];
} Vec3;

typedef union {
    struct { Vec3 x, y, z; };
    Vec3 cols[3];
    float nums[3][3];
} Mat3;

static Mat3 mul3x3(Mat3 a, Mat3 b) {
    Mat3 out = {0};
    int k, r, c;
    for (c = 0; c < 3; ++c)
        for (r = 0; r < 3; ++r) {
            out.nums[c][r] = 0.0f;
            for (k = 0; k < 3; ++k)
                out.nums[c][r] += a.nums[k][r] * b.nums[c][k];
        }
    return out;
}

static Vec3 mul3x33(Mat3 m, Vec3 v) {
    Vec3 res;
    for(int x = 0; x < 3; ++x) {
        float sum = 0;
        for(int y = 0; y < 3; ++y)
            sum += m.nums[y][x] * v.nums[y];

        res.nums[x] = sum;
    }
    return res;
}


static Mat3 ident3x3(void) {
    Mat3 res = {0};
    res.nums[0][0] = 1.0f;
    res.nums[1][1] = 1.0f;
    res.nums[2][2] = 1.0f;
    return res;
}


static Mat3 scale3x3(Vec2 scale) {
    Mat3 res = ident3x3();
    res.nums[0][0] = scale.x;
    res.nums[1][1] = scale.y;
    return res;
}

static Mat3 translate3x3(Vec2 pos) {
    Mat3 res = ident3x3();
    res.nums[2][0] = pos.x;
    res.nums[2][1] = pos.y;
    return res;
}

static Mat3 rotate3x3(float rot) {
    Mat3 res = ident3x3();
    res.nums[0][0] = cosf(rot);
    res.nums[0][1] = -sinf(rot);
    res.nums[1][0] = sinf(rot);
    res.nums[1][1] = cosf(rot);
    return res;
}

static Mat3 ortho3x3(float left, float right, float bottom, float top) {
    Mat3 res = ident3x3();

    res.nums[0][0] = 2.0f / (right - left);
    res.nums[1][1] = 2.0f / (top - bottom);
    res.nums[2][2] = 1.0f;

    res.nums[2][0] = (left + right) / (left - right);
    res.nums[2][1] = (bottom + top) / (bottom - top);

    return res;
}
