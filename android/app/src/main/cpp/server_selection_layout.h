#pragma once

#include <cstddef>
#include <vector>

namespace nevergone::server_selection_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kFirstColumnX = 56.0f;
constexpr float kSecondColumnExtraX = 96.0f;
constexpr float kFirstRowCenterY = 320.0f;
constexpr float kRowStepY = 90.0f;

struct RowRect {
    int index = -1;       // project-owned zero-based index
    int original_tag = 0; // original NewServerList row tag, 1..N
    float left = 0.0f;
    float bottom = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Reconstruct the row placement from NewServerList::init. Row sprite size is
// supplied by the caller so the same geometry works both with the recovered
// expansion border2.png content size and with the project-owned fallback when
// expansion assets have not been imported. scroll_offset_y mirrors the content
// layer's Y translation observed by NewServerList::GetDrawRectSp.
RowRect row_rect(
    std::size_t index,
    float row_width,
    float row_height,
    float scroll_offset_y = 0.0f);

std::vector<RowRect> build_rows(
    std::size_t server_count,
    float row_width,
    float row_height,
    float scroll_offset_y = 0.0f);

bool contains(const RowRect& row, float x, float y);
int hit_test(
    std::size_t server_count,
    float row_width,
    float row_height,
    float x,
    float y,
    float scroll_offset_y = 0.0f);

}  // namespace nevergone::server_selection_layout
