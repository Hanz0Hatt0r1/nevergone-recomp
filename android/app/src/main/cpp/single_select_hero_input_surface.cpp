#include "single_select_hero_input.h"

#include <sstream>
#include <string>

#include "character_name_state.h"
#include "offline_startup_flow.h"
#include "render_bridge.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_input {
namespace {

bool route_active() {
    return offline_startup_flow::snapshot().route ==
            offline_startup_flow::Route::kOpeningDialogue &&
        single_select_hero_state::snapshot().active &&
        !character_name_state::snapshot().active;
}

bool read_render_surface_size(int* width, int* height) {
    if (width == nullptr || height == nullptr) return false;
    const std::string report = render::status_report();
    static constexpr const char* kPrefix = "render surface: ";
    const std::size_t start = report.find(kPrefix);
    if (start == std::string::npos) return false;
    const std::size_t value_start = start + std::char_traits<char>::length(kPrefix);
    const std::size_t line_end = report.find('\n', value_start);
    const std::string value = report.substr(value_start, line_end - value_start);
    const std::size_t separator = value.find('x');
    if (separator == std::string::npos) return false;

    std::istringstream width_stream(value.substr(0, separator));
    std::istringstream height_stream(value.substr(separator + 1));
    int parsed_width = 0;
    int parsed_height = 0;
    width_stream >> parsed_width;
    height_stream >> parsed_height;
    if (!width_stream || !height_stream || parsed_width <= 0 || parsed_height <= 0) {
        return false;
    }
    *width = parsed_width;
    *height = parsed_height;
    return true;
}

}  // namespace

bool on_touch(int action, int pointer_id, float x, float y) {
    if (!route_active()) {
        reset();
        return false;
    }

    int width = 0;
    int height = 0;
    if (!read_render_surface_size(&width, &height)) {
        reset();
        return true;
    }
    return on_touch_for_surface(action, pointer_id, x, y, width, height);
}

}  // namespace nevergone::single_select_hero_input
