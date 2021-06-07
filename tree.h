static void treegen_recursive(BoxId box, int n) {
    if (n < 0) return;

    BoxId left = add_box(box, Face_Left, BoxKind_Dirt);
    treegen_recursive(left, n - 1);

    BoxId right = add_box(box, Face_Right, BoxKind_Dirt);
    treegen_recursive(right, n - 1);

    BoxId front = add_box(box, Face_Front, BoxKind_Dirt);
    treegen_recursive(front, n - 1);

    BoxId back = add_box(box, Face_Back, BoxKind_Dirt);
    treegen_recursive(back, n - 1);

    BoxId above = add_box(box, Face_Above, BoxKind_Dirt);
    treegen_recursive(above, n - 1);
}