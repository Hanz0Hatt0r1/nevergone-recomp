#include <cassert>
#include <cmath>

#include "single_select_hero_character_layout.h"

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
}

int main() {
    namespace preview = nevergone::single_select_hero_character_layout;
    assert(preview::frame_index(1, 0) == 0);
    assert(preview::frame_index(1, 1) == 1);
    assert(preview::frame_index(1, 2) == 0);
    assert(preview::frame_index(2, 0) == 2);
    assert(preview::frame_index(2, 1) == 2);
    assert(preview::frame_index(2, 2) == 3);
    assert(preview::frame_index(3, 0) == -1);

    // Shipped Singleselechero.plist: P01DL untrimmed 218x465.
    preview::FrameGeometry dawn{218, 465, 0, 0, 218, 465};
    auto q = preview::quad_for_surface(dawn, 1136, 640);
    assert(q.valid);
    const float left = preview::kPreviewCenterX - 109.0f;
    const float top = 640.0f - preview::kPreviewCenterY - 232.5f;
    assert(near(q.x0, left * 2.0f / 1136.0f - 1.0f));
    assert(near(q.y0, 1.0f - top * 2.0f / 640.0f));

    // Shipped P02DL frame uses TexturePacker trim {13,11} {204,439}.
    preview::FrameGeometry higgs{204, 439, 13, 11, 218, 465};
    auto p = preview::quad_for_surface(higgs, 1136, 640);
    assert(p.valid);
    assert(p.x0 > q.x0);
    assert(p.y0 < q.y0);
    assert(p.x1 < q.x1);
    assert(p.y1 > q.y1);

    auto scaled = preview::quad_for_surface(higgs, 2272, 1280);
    assert(scaled.valid && near(scaled.x0, p.x0) && near(scaled.y1, p.y1));
    auto letterbox = preview::quad_for_surface(higgs, 1920, 1200);
    assert(letterbox.valid && letterbox.x0 > -1.0f && letterbox.x1 < 1.0f);
    assert(!preview::quad_for_surface(higgs, 0, 640).valid);
    auto invalid = higgs;
    invalid.left = 40;
    assert(!preview::quad_for_surface(invalid, 1136, 640).valid);
    return 0;
}
