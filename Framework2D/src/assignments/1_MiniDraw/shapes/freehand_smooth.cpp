#include "freehand_smooth.h"
#include <imgui.h>
#include <math.h>

namespace USTC_CG
{
    FreehandSmooth::FreehandSmooth()
    {
    }
    
    FreehandSmooth::FreehandSmooth(std::vector<float> x_list, std::vector<float> y_list)
        : x_list_(x_list), y_list_(y_list)
    {
    }
    
    void FreehandSmooth::draw(const Config& config) const
    {
        if (x_list_.empty() || y_list_.empty() || x_list_.size() != y_list_.size())
            return;
            
        // Use smooth drawing
        draw_smooth(config);
    }
    
    void FreehandSmooth::update(float x, float y)
    {
        if (!x_list_.empty() && !y_list_.empty()) {
            x_list_.back() = x;
            y_list_.back() = y;
        }
    }
    
    void FreehandSmooth::add_control_point(float x, float y)
    {
        // Only add a new point when the distance to the last point is large enough
        if (x_list_.empty() || y_list_.empty() || 
            distance(x_list_.back(), y_list_.back(), x, y) >= MIN_SAMPLE_DISTANCE) {
            x_list_.push_back(x);
            y_list_.push_back(y);
        } else {
            // Update the last point's position
            x_list_.back() = x;
            y_list_.back() = y;
        }
    }
    
    void FreehandSmooth::force_add_control_point(float x, float y)
    {
        // Always add a new point, regardless of distance
        x_list_.push_back(x);
        y_list_.push_back(y);
    }
    
    float FreehandSmooth::distance(float x1, float y1, float x2, float y2) const
    {
        float dx = x2 - x1;
        float dy = y2 - y1;
        return sqrt(dx * dx + dy * dy);
    }
    
    void FreehandSmooth::draw_smooth(const Config& config) const
    {
        if (x_list_.empty() || y_list_.empty() || x_list_.size() != y_list_.size())
            return;
            
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        
        // If we have only one point, draw a small circle
        if (x_list_.size() == 1) {
            ImVec2 center(config.bias[0] + x_list_[0], config.bias[1] + y_list_[0]);
            draw_list->AddCircleFilled(
                center,
                line_thickness_ / 2.0f,
                IM_COL32(
                    color_[0],
                    color_[1],
                    color_[2],
                    255
                )
            );
            return;
        }
        
        // If we have only two points, draw a simple line
        if (x_list_.size() == 2) {
            ImVec2 p0(config.bias[0] + x_list_[0], config.bias[1] + y_list_[0]);
            ImVec2 p1(config.bias[0] + x_list_[1], config.bias[1] + y_list_[1]);
            draw_list->AddLine(
                p0, p1,
                IM_COL32(
                    color_[0],
                    color_[1],
                    color_[2],
                    255
                ),
                line_thickness_
            );
            return;
        }
        
        // Use Bezier curves to smoothly connect points
        for (size_t i = 0; i < x_list_.size() - 1; i++) {
            ImVec2 p0(config.bias[0] + x_list_[i], config.bias[1] + y_list_[i]);
            ImVec2 p3(config.bias[0] + x_list_[i+1], config.bias[1] + y_list_[i+1]);
            
            // Calculate control points
            ImVec2 p1, p2;
            
            if (i == 0) {
                // First segment
                p1 = p0;
                p2 = ImVec2(
                    config.bias[0] + (x_list_[i] + x_list_[i+1]) / 2,
                    config.bias[1] + (y_list_[i] + y_list_[i+1]) / 2
                );
            } else if (i == x_list_.size() - 2) {
                // Last segment
                p1 = ImVec2(
                    config.bias[0] + (x_list_[i-1] + x_list_[i]) / 2,
                    config.bias[1] + (y_list_[i-1] + y_list_[i]) / 2
                );
                p2 = p3;
            } else {
                // Middle segments
                p1 = ImVec2(
                    config.bias[0] + (x_list_[i-1] + x_list_[i]) / 2,
                    config.bias[1] + (y_list_[i-1] + y_list_[i]) / 2
                );
                p2 = ImVec2(
                    config.bias[0] + (x_list_[i] + x_list_[i+1]) / 2,
                    config.bias[1] + (y_list_[i] + y_list_[i+1]) / 2
                );
            }
            
            // Draw Bezier curve
            draw_list->AddBezierCubic(
                p0, p1, p2, p3,
                IM_COL32(
                    color_[0],
                    color_[1],
                    color_[2],
                    255
                ),
                line_thickness_,
                0
            );
        }
    }
}