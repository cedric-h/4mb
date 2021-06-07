static void add_tree(int16_t x, int16_t y, int16_t z) {
    int16_t i = 17;

    boxes[i] = (Box) {
        .kind = BoxKind_Dirt,
        .pos.x = x,
        .pos.y = y,
        .pos.z = z,
    };
    
    // LOG
    add_box(i, Face_Above, BoxKind_Dirt);
    add_box(i+1, Face_Above, BoxKind_Dirt);

    // LEAVES
    add_box(i+2, Face_Left, BoxKind_Dirt);
    add_box(i+2, Face_Right, BoxKind_Dirt);
    add_box(i+2, Face_Front, BoxKind_Dirt);
    add_box(i+2, Face_Back, BoxKind_Dirt);
    add_box(i+2, Face_Above, BoxKind_Dirt);
}