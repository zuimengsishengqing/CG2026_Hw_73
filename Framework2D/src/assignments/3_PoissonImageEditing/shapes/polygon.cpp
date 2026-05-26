#include "polygon.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <imgui.h>

namespace USTC_CG
{

void Polygon::draw(const Config& config) const
{
    if (vertices_.size() < 2)
        return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 offset(config.bias[0], config.bias[1]);
    ImU32 color = IM_COL32(
        config.line_color[0],
        config.line_color[1],
        config.line_color[2],
        config.line_color[3]);

    for (size_t i = 0; i < vertices_.size(); ++i)
    {
        size_t next = (i + 1) % vertices_.size();
        ImVec2 p1(vertices_[i].first + offset.x, vertices_[i].second + offset.y);
        ImVec2 p2(vertices_[next].first + offset.x, vertices_[next].second + offset.y);
        draw_list->AddLine(p1, p2, color, config.line_thickness);
    }
}

void Polygon::update(float x, float y)
{
}

void Polygon::add_control_point(float x, float y)
{
    vertices_.push_back({ x, y });
}

std::vector<std::pair<int, int>> Polygon::get_interior_pixels() const
{
    std::vector<std::pair<int, int>> interior_pixels;

    if (vertices_.size() < 3)
        return interior_pixels;

    int min_y = static_cast<int>(std::floor(vertices_[0].second));
    int max_y = static_cast<int>(std::floor(vertices_[0].second));

    for (const auto& vertex : vertices_)
    {
        int y = static_cast<int>(std::floor(vertex.second));
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }

    int estimated_pixels = (max_y - min_y + 1) * (max_y - min_y + 1);
    interior_pixels.reserve(estimated_pixels / 4);

    std::map<int, std::vector<Edge>> edge_table;

    for (size_t i = 0; i < vertices_.size(); ++i)
    {
        size_t next = (i + 1) % vertices_.size();
        const auto& v1 = vertices_[i];
        const auto& v2 = vertices_[next];

        int y1 = static_cast<int>(std::floor(v1.second));
        int y2 = static_cast<int>(std::floor(v2.second));

        if (y1 == y2)
            continue;

        int y_min = std::min(y1, y2);
        int y_max = std::max(y1, y2);

        float x_start = (y_min == y1) ? v1.first : v2.first;
        float x_end = (y_min == y1) ? v2.first : v1.first;

        float inv_k = (x_end - x_start) / (y_max - y_min);

        Edge edge;
        edge.y_max = y_max;
        edge.x_curr = x_start;
        edge.inv_k = inv_k;

        edge_table[y_min].push_back(edge);
    }

    for (auto& [y, edges] : edge_table)
    {
        std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
            return a.x_curr < b.x_curr;
        });
    }

    std::vector<Edge> aet;

    for (int y = min_y; y <= max_y; ++y)
    {
        if (edge_table.find(y) != edge_table.end())
        {
            for (const auto& edge : edge_table[y])
            {
                if (edge.y_max > y)
                    aet.push_back(edge);
            }
        }

        aet.erase(
            std::remove_if(
                aet.begin(),
                aet.end(),
                [y](const Edge& edge) { return edge.y_max <= y; }),
            aet.end());

        std::sort(
            aet.begin(),
            aet.end(),
            [](const Edge& a, const Edge& b) { return a.x_curr < b.x_curr; });

        for (size_t i = 0; i + 1 < aet.size(); i += 2)
        {
            int x_start = static_cast<int>(std::ceil(aet[i].x_curr));
            int x_end = static_cast<int>(std::floor(aet[i + 1].x_curr));

            if (x_start <= x_end)
            {
                for (int x = x_start; x <= x_end; ++x)
                {
                    interior_pixels.push_back({ x, y });
                }
            }
        }

        if (aet.size() % 2 != 0)
        {
            int x = static_cast<int>(std::round(aet.back().x_curr));
            interior_pixels.push_back({ x, y });
        }

        for (auto& edge : aet)
        {
            edge.x_curr += edge.inv_k;
        }
    }

    return interior_pixels;
}

}  // namespace USTC_CG