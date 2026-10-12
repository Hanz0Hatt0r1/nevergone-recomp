#pragma once

#include <cstddef>
#include <cstdint>

namespace nevergone::tutorial_loading_compositor {

void on_surface_created();
void on_surface_changed(int width, int height);
void clear_frames();
bool upload_frame(
    int frame_index,
    int width,
    int height,
    const std::uint32_t* argb,
    size_t count);
void draw();

}  // namespace nevergone::tutorial_loading_compositor
