#include "server_selection_layout.h"

#include <cmath>
#include <limits>

namespace nevergone::server_selection_layout {
namespace {

bool valid_dimension(float value) {
    return std::isfinite(value) && value > 0.0f;
}

bool valid_coordinate(float value) {
    return std::isfinite(value);
}

}  // namespace

RowRect row_rect(
    std::size_t index,
    float row_width,
    float row_height,
    float scroll_offset_y) {
    RowRect result;
    if (!valid_dimension(row_width) || !valid_dimension(row_height) ||
            !valid_coordinate(scroll_offset_y) ||
            index > static_cast<std::size_t>(std::numeric_limits<int>::max() - 1)) {
        return result;
    }

    const std::size_t column = index % 2u;
    const std::size_t row = index / 2u;
    const float left = column == 0u
        ? kFirstColumnX
        : row_width + kSecondColumnExtraX;
    const float center_y = kFirstRowCenterY -
        kRowStepY * static_cast<float>(row) + scroll_offset_y;

    result.index = static_cast<int>(index);
    result.original_tag = result.index + 1;
    result.left = left;
    result.bottom = center_y - row_height * 0.5f;
    result.width = row_width;
    result.height = row_height;
    return result;
}

std::vector<RowRect> build_rows(
    std::size_t server_count,
    float row_width,
    float row_height,
    float scroll_offset_y) {
    std::vector<RowRect> rows;
    if (!valid_dimension(row_width) || !valid_dimension(row_height) ||
            !valid_coordinate(scroll_offset_y)) {
        return rows;
    }
    rows.reserve(server_count);
    for (std::size_t index = 0; index < server_count; ++index) {
        RowRect row = row_rect(index, row_width, row_height, scroll_offset_y);
        if (row.index < 0) break;
        rows.push_back(row);
    }
    return rows;
}

bool contains(const RowRect& row, float x, float y) {
    if (row.index < 0 || !valid_dimension(row.width) || !valid_dimension(row.height) ||
            !valid_coordinate(x) || !valid_coordinate(y)) {
        return false;
    }
    return x >= row.left && x <= row.left + row.width &&
        y >= row.bottom && y <= row.bottom + row.height;
}

int hit_test(
    std::size_t server_count,
    float row_width,
    float row_height,
    float x,
    float y,
    float scroll_offset_y) {
    if (!valid_coordinate(x) || !valid_coordinate(y)) return -1;
    const auto rows = build_rows(server_count, row_width, row_height, scroll_offset_y);
    for (const RowRect& row : rows) {
        if (contains(row, x, y)) return row.index;
    }
    return -1;
}

}  // namespace nevergone::server_selection_layout
