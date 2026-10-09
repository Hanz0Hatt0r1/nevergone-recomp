#include <cassert>
#include <cmath>

#include "server_selection_layout.h"

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.001f;
}

}  // namespace

int main() {
    using namespace nevergone::server_selection_layout;

    constexpr float width = 300.0f;
    constexpr float height = 64.0f;

    const RowRect row0 = row_rect(0, width, height);
    assert(row0.index == 0);
    assert(row0.original_tag == 1);
    assert(near(row0.left, 56.0f));
    assert(near(row0.bottom, 288.0f));
    assert(near(row0.width, width));
    assert(near(row0.height, height));

    const RowRect row1 = row_rect(1, width, height);
    assert(row1.original_tag == 2);
    assert(near(row1.left, 396.0f));
    assert(near(row1.bottom, 288.0f));

    const RowRect row2 = row_rect(2, width, height);
    assert(row2.original_tag == 3);
    assert(near(row2.left, 56.0f));
    assert(near(row2.bottom, 198.0f));

    const RowRect row5 = row_rect(5, width, height);
    assert(row5.original_tag == 6);
    assert(near(row5.left, 396.0f));
    assert(near(row5.bottom, 108.0f));

    // GetDrawRectSp adds the scroll content layer's Y position for normal rows.
    const RowRect scrolled = row_rect(2, width, height, 25.0f);
    assert(near(scrolled.bottom, 223.0f));

    const auto rows = build_rows(7, width, height);
    assert(rows.size() == 7);
    assert(rows.back().index == 6);
    assert(rows.back().original_tag == 7);
    assert(near(rows.back().left, 56.0f));
    assert(near(rows.back().bottom, 18.0f));

    assert(contains(row0, 56.0f, 288.0f));
    assert(contains(row0, 356.0f, 352.0f));
    assert(!contains(row0, 55.99f, 320.0f));
    assert(!contains(row0, 100.0f, 352.01f));

    assert(hit_test(6, width, height, 100.0f, 320.0f) == 0);
    assert(hit_test(6, width, height, 500.0f, 320.0f) == 1);
    assert(hit_test(6, width, height, 100.0f, 230.0f) == 2);
    assert(hit_test(6, width, height, 500.0f, 140.0f) == 5);
    assert(hit_test(6, width, height, 20.0f, 320.0f) == -1);
    assert(hit_test(6, width, height, 100.0f, 255.0f, 25.0f) == 2);

    // The user-supplied expansion assets and shipped ARMv7 init path identify
    // border2.png as the selectable row sprite. Its decoded content size is
    // 499x68, so the recovered second-column X becomes 499+96 = 595.
    constexpr float obb_width = 499.0f;
    constexpr float obb_height = 68.0f;
    const RowRect obb0 = row_rect(0, obb_width, obb_height);
    const RowRect obb1 = row_rect(1, obb_width, obb_height);
    const RowRect obb2 = row_rect(2, obb_width, obb_height);
    assert(near(obb0.left, 56.0f));
    assert(near(obb0.bottom, 286.0f));
    assert(near(obb1.left, 595.0f));
    assert(near(obb1.bottom, 286.0f));
    assert(near(obb2.bottom, 196.0f));
    assert(hit_test(4, obb_width, obb_height, 56.0f, 320.0f) == 0);
    assert(hit_test(4, obb_width, obb_height, 595.0f, 320.0f) == 1);
    assert(hit_test(4, obb_width, obb_height, 594.99f, 320.0f) == -1);

    assert(row_rect(0, 0.0f, height).index == -1);
    assert(build_rows(3, width, -1.0f).empty());
    assert(hit_test(3, width, height, NAN, 0.0f) == -1);

    return 0;
}
