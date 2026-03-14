#include "line.h"

#include <imgui.h>

namespace USTC_CG
{
// Draw the line using ImGui
void Line::draw(const Config& config) const
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    draw_list->AddLine(
            ImVec2(
                config.bias[0] + start_point_x_, config.bias[1] + start_point_y_),
            ImVec2(config.bias[0] + end_point_x_, config.bias[1] + end_point_y_),
            IM_COL32(
                color_[0],
                color_[1],
                color_[2],
                255),
            line_thickness_);
}

void Line::update(float x, float y)
{
    end_point_x_ = x;
    end_point_y_ = y;
}
}  // namespace USTC_CG