#include "freehand.h"
#include <imgui.h>
#include <math.h>

namespace USTC_CG{
    Freehand::Freehand()
    {

    }
    Freehand::Freehand(std::vector<float> x_list, std::vector<float> y_list)
        : x_list_(x_list), y_list_(y_list)
        {
        }
    
    void Freehand::draw(const Config& config) const
    {
        if (x_list_.empty() || y_list_.empty() || x_list_.size() != y_list_.size())
            return;

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        //Polygon Points
        std::vector<ImVec2> points;
        for(size_t i = 0; i < x_list_.size(); i++){
            points.push_back(ImVec2(
                config.bias[0] + x_list_[i],
                config.bias[1] + y_list_[i]
            ));
        }
        
        draw_list->AddPolyline(
            points.data(),
            static_cast<int>(points.size()),
            IM_COL32(
                color_[0],
                color_[1],
                color_[2],
                255
            ),
            //freehand dont need close
            ImDrawFlags_None,
            line_thickness_
        );

    };

    void Freehand::update(float x, float y)
    {
        if(!x_list_.empty() && !y_list_.empty()){
            x_list_.back() = x;
            y_list_.back() = y;
        }
    }
    void Freehand::add_control_point(float x, float y)
    {
        x_list_.push_back(x);
        y_list_.push_back(y);
    }



    
}