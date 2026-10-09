#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

#include "../android/app/src/main/cpp/character_name_assets.h"
#include "../android/app/src/main/cpp/character_name_input.h"

namespace {

std::array<int, 8> g_tags{};
int g_count = 0;

void record_tag(int tag) {
    if (g_count < static_cast<int>(g_tags.size())) g_tags[g_count++] = tag;
}

void upload_image(int slot, int width, int height) {
    const std::uint32_t pixel = 0xffffffffu;
    std::vector<std::uint32_t> pixels(
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height), pixel);
    assert(nevergone::character_name_assets::upload_image(
        slot, width, height, pixels.data(), pixels.size()));
}

void upload_label(int slot) {
    const std::uint32_t pixel = 0xffffffffu;
    assert(nevergone::character_name_assets::upload_label(slot, 1, 1, &pixel, 1));
}

void stage_assets() {
    namespace assets = nevergone::character_name_assets;
    assets::clear();
    upload_image(static_cast<int>(assets::ImageSlot::kBackground), 200, 100);
    upload_image(static_cast<int>(assets::ImageSlot::kNamePlate), 300, 50);
    upload_image(static_cast<int>(assets::ImageSlot::kRandomButton), 60, 40);
    upload_image(static_cast<int>(assets::ImageSlot::kFixedButtonNormal), 100, 50);
    upload_image(static_cast<int>(assets::ImageSlot::kFixedButtonPressed), 100, 50);
    upload_image(static_cast<int>(assets::ImageSlot::kFixedButtonDisabled), 100, 50);
    for (int slot = 0; slot < assets::kLabelCount; ++slot) upload_label(slot);
    assert(assets::ready());
}

}  // namespace

int main() {
    namespace input = nevergone::character_name_input;
    namespace assets = nevergone::character_name_assets;

    input::reset();
    assert(!input::on_touch_for_surface(0, 1, 1001.0f, 588.0f, 1136, 640, false, &record_tag));

    assets::clear();
    assert(input::on_touch_for_surface(0, 1, 1001.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::pressed_tag() == 0);

    stage_assets();

    // Confirm: recovered center is (1001, 52) in bottom-origin design space.
    assert(input::on_touch_for_surface(0, 7, 1001.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::pressed_tag() == 1);
    assert(input::on_touch_for_surface(1, 7, 1001.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::pressed_tag() == 0);
    assert(g_count == 1 && g_tags[0] == 1);

    // Cancel at (135, 52).
    assert(input::on_touch_for_surface(0, 8, 135.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::on_touch_for_surface(1, 8, 135.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(g_count == 2 && g_tags[1] == 2);

    // Random center: name_y = 367, x = 758 with the staged dimensions.
    assert(input::on_touch_for_surface(0, 9, 758.0f, 273.0f, 1136, 640, true, &record_tag));
    assert(input::pressed_tag() == 3);
    assert(input::on_touch_for_surface(1, 9, 758.0f, 273.0f, 1136, 640, true, &record_tag));
    assert(g_count == 3 && g_tags[2] == 3);

    // Moving outside an armed control cancels activation.
    assert(input::on_touch_for_surface(0, 10, 1001.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::on_touch_for_surface(2, 10, 568.0f, 320.0f, 1136, 640, true, &record_tag));
    assert(input::pressed_tag() == 0);
    assert(input::on_touch_for_surface(1, 10, 568.0f, 320.0f, 1136, 640, true, &record_tag));
    assert(g_count == 3);

    // Modal background is consumed and first pointer owns the gesture.
    assert(input::on_touch_for_surface(0, 11, 568.0f, 320.0f, 1136, 640, true, &record_tag));
    assert(input::on_touch_for_surface(5, 12, 1001.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::on_touch_for_surface(6, 12, 1001.0f, 588.0f, 1136, 640, true, &record_tag));
    assert(input::on_touch_for_surface(1, 11, 568.0f, 320.0f, 1136, 640, true, &record_tag));
    assert(g_count == 3);

    // Aspect-fit mapping remains exact at 2x.
    assert(input::on_touch_for_surface(0, 13, 2002.0f, 1176.0f, 2272, 1280, true, &record_tag));
    assert(input::on_touch_for_surface(1, 13, 2002.0f, 1176.0f, 2272, 1280, true, &record_tag));
    assert(g_count == 4 && g_tags[3] == 1);

    input::reset();
    return 0;
}
