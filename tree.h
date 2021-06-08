static void treegen_recursive(BoxId box, int n, Vec3 center) {
    if (n < 0) return;

    Vec3 box_pos = box_pos_to_vec3(boxes[box].pos);    
    if (sdf_sphere3(sub3(box_pos, center), 2.0f) > 0.0f) return;

    BoxId left = add_box(box, Face_Left, BoxKind_Dirt);
    treegen_recursive(left, n - 1, center);

    BoxId right = add_box(box, Face_Right, BoxKind_Dirt);
    treegen_recursive(right, n - 1, center);

    BoxId front = add_box(box, Face_Front, BoxKind_Dirt);
    treegen_recursive(front, n - 1, center);

    BoxId back = add_box(box, Face_Back, BoxKind_Dirt);
    treegen_recursive(back, n - 1, center);

    BoxId above = add_box(box, Face_Above, BoxKind_Dirt);
    treegen_recursive(above, n - 1, center);
}