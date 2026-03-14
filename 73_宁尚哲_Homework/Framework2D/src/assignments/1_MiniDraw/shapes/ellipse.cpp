#include "ellipse.h"
#include "math.h"
#include <imgui.h>

namespace USTC_CG{
void Ellipse::draw(const Config& config) const
{
    //
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // Calculate center and radius from start and end points
    float center_x = (config.bias[0] + start_point_x_ + config.bias[0] + end_point_x_) / 2.0f;
    float center_y = (config.bias[1] + start_point_y_ + config.bias[1] + end_point_y_) / 2.0f;
    float radius_x = fabs((config.bias[0] + end_point_x_ - config.bias[0] - start_point_x_) / 2.0f);
    float radius_y = fabs((config.bias[1] + end_point_y_ - config.bias[1] - start_point_y_) / 2.0f);
    
    draw_list->AddEllipse(
        ImVec2(center_x, center_y),  // center
        ImVec2(radius_x, radius_y), // radius
        IM_COL32(
            color_[0],
            color_[1],
            color_[2],
            255),
        0.f,  // rotation
        0,    // num_segments (0 = auto)
        line_thickness_
    );
}
void Ellipse::update(float x, float y)
{
    end_point_x_ = x;
    end_point_y_ = y;
}
}