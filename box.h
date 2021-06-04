typedef struct { int16_t x, y, z; } BoxPos;
static Vec3 box_pos_to_vec3(BoxPos bp) {
    return vec3(bp.x, bp.y, bp.z);
}
static BoxPos add_bp(BoxPos a, BoxPos b) {
    return (BoxPos) { a.x + b.x,
                      a.y + b.y,
                      a.z + b.z, };
}
static int eq_bp(BoxPos a, BoxPos b) {
    return a.x == b.x &&
           a.y == b.y &&
           a.z == b.z;
}

typedef enum { BoxKind_Unoccupied, BoxKind_Dirt } BoxKind;
#define OCCUPIED(box) ((box).kind != BoxKind_Unoccupied)

typedef enum {
    Face_Left,  Face_Right,
    Face_Above, Face_Below,
    Face_Front, Face_Back,
    Face_COUNT
} Face;
const Face face_opposite[Face_COUNT] = {
    Face_Right, Face_Left,  
    Face_Below, Face_Above, 
    Face_Back,  Face_Front, 
};
const BoxPos face_offset[Face_COUNT] = {
    { 1,  0,  0}, {-1,  0,  0},
    { 0,  1,  0}, { 0, -1,  0},
    { 0,  0,  1}, { 0,  0, -1},
};

#define BoxId uint16_t
#define BoxId_NULL (0)
typedef struct {
    /* records indexes of Boxes that touch faces */
    BoxId touching[Face_COUNT];
    BoxPos pos;
    BoxKind kind;
} Box;

#define MAX_BOXES (2 << 10)
static Box boxes[MAX_BOXES];

static void rem_box(BoxId bye_id) {
    Box *bye = boxes + bye_id;
    for (Face f = 0; f < Face_COUNT; f++)
        boxes[bye->touching[f]].touching[face_opposite[f]] = BoxId_NULL;
    *bye = (Box) {0};
}

static BoxId add_box(BoxId onto_id, Face face, BoxKind kind) {
    Box *onto = boxes + onto_id;

    /* no adding BoxKind_Unoccupied boxes, that's weird */
    if (kind == BoxKind_Unoccupied) return BoxId_NULL;

    /* no adding onto an unoccupied box, that's weird */
    if (!OCCUPIED(*onto)) return BoxId_NULL;

    /* no writing over a face this box already has */
    if (onto->touching[face] != BoxId_NULL) return BoxId_NULL;

    BoxId new_box_id = BoxId_NULL;
    for (BoxId id = 1; id < MAX_BOXES; id++)
        if (!OCCUPIED(boxes[id])) {
            new_box_id = id;
            break;
        }
    /* dayum, you done used all the boxes up
       TODO: reallocate or something here? */
    if (new_box_id == BoxId_NULL) return BoxId_NULL;

    Box new_box = (Box) { .kind = kind };
    new_box.pos = add_bp(onto->pos, face_offset[face]);
    new_box.touching[face_opposite[face]] = onto_id;

    /* TODO: this, without iterating through ALL of the boxes */
    for (BoxId id = 1; id < MAX_BOXES; id++)
        if (OCCUPIED(boxes[id]))
            for (Face f = 0; f < Face_COUNT; f++)
                if (eq_bp(boxes[id].pos, add_bp(new_box.pos, face_offset[f]))) {
                    BoxId *touch = boxes[id].touching + face_opposite[f];

                    /* somehow one of this would-be boxes's neighbors already
                       has a neighbor in this spot, but the box we're building
                       off of wasn't informed of that. this means that the
                       linked list hasn't been maintained properly, so you're
                       really in deep if this happens */
                    if (*touch != BoxId_NULL) {
                        log_err("Critical linked list error found adding new block");
                        return BoxId_NULL;
                    }

                    *touch = new_box_id;
                    new_box.touching[f] = id;
                }

    *(boxes + new_box_id) = new_box;
    return new_box_id;
}

static Face box_ray_face(Vec3 ro, Vec3 rd, Vec3 rad) {
    Vec3 m = div3(vec3_f(1.0f), rd);
    Vec3 n = mul3(m, ro);
    Vec3 k = mul3(abs3(m), rad);
    Vec3 t1 = sub3(mul3_f(n, -1.0f), k);
    Vec3 t2 = add3(mul3_f(n, -1.0f), k);

    BoxPos offset = {
        .x = (int16_t) (sign(rd.x) * step(t1.y, t1.x) * step(t1.z, t1.x)),
        .y = (int16_t) (sign(rd.y) * step(t1.z, t1.y) * step(t1.x, t1.y)),
        .z = (int16_t) (sign(rd.z) * step(t1.x, t1.z) * step(t1.y, t1.z)),
    };

    for (Face f = 0; f < Face_COUNT; f++)
        if (eq_bp(offset, face_offset[f]))
            return f;
    
    log_err("box_ray_face produced invalid offset");
    return Face_COUNT;
}

static float box_ray_dist(Vec3 ro, Vec3 rd, Vec3 rad) {
    Vec3 m = div3(vec3_f(1.0f), rd);
    Vec3 n = mul3(m, ro);
    Vec3 k = mul3(abs3(m), rad);
    Vec3 t1 = sub3(mul3_f(n, -1.0f), k);
    Vec3 t2 = add3(mul3_f(n, -1.0f), k);

    float i_near = fmaxf(fmaxf(t1.x, t1.y), t1.z);
    float i_far  = fminf(fminf(t2.x, t2.y), t2.z);
	
    return (i_near > i_far || i_far < 0.0) ? INFINITY : i_near;
}


/* if face is not a NULL pointer, the face that was hit will be written into it */
static BoxId box_under_ray(Vec3 p, Vec3 rd, Face *face) {
    float res_dist = INFINITY;
    Vec3 res_ro = {0};
    BoxId res = BoxId_NULL;
    for (BoxId id = 1; id < MAX_BOXES; id++) if (OCCUPIED(boxes[id])) {
        Vec3 pos = box_pos_to_vec3(boxes[id].pos);
        Vec3 ro = sub3(p, add3_f(pos, 0.5f));
        float this_dist = box_ray_dist(ro, rd, vec3_f(0.5f));
        if (this_dist < res_dist) {
            res_dist = this_dist;
            res = id;
            res_ro = ro;
        }
    }

    if (res != BoxId_NULL && face != NULL)
        *face = box_ray_face(res_ro, rd, vec3_f(0.5f));
    return res;
}
