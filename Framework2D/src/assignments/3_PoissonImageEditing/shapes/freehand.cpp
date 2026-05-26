#include "freehand.h"

#include <imgui.h>

namespace USTC_CG
{

void Freehand::draw(const Config& config) const
{
    if (get_vertices().size() < 2)
        return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 offset(config.bias[0], config.bias[1]);
    ImU32 color = IM_COL32(
        config.line_color[0],
        config.line_color[1],
        config.line_color[2],
        config.line_color[3]);

    const auto& vertices = get_vertices();
    
    for (size_t i = 0; i < vertices.size() - 1; ++i)
    {
        ImVec2 p1(vertices[i].first + offset.x, vertices[i].second + offset.y);
        ImVec2 p2(vertices[i + 1].first + offset.x, vertices[i + 1].second + offset.y);
        draw_list->AddLine(p1, p2, color, config.line_thickness);
    }
}

void Freehand::update(float x, float y)
{
    add_control_point(x, y);
}

}  // namespace USTC_CG