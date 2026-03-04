#include "polygon.h"
#include "math.h"
#include <imgui.h>

namespace USTC_CG{
    Polygon::Polygon()
    {
    }
    Polygon::Polygon(std::vector<float> xlist, std::vector<float> y_list)
        : x_list_(xlist), y_list_(y_list)
        {
        }
    void Polygon::draw(const Config& config) const
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
                config.line_color[0],
                config.line_color[1],
                config.line_color[2],
                config.line_color[3]
            ),
            //判定闭合补齐
            is_closed_ ? ImDrawFlags_Closed : ImDrawFlags_None,
            config.line_thickness
        
        );

    };

    void Polygon::update(float x, float y)
    {
        //更新最新的点坐标
        if(!x_list_.empty() && !y_list_.empty()){
            x_list_.back() = x;
            y_list_.back() = y;
        }
    }
    void Polygon::add_control_point(float x, float y)
    {
        x_list_.push_back(x);
        y_list_.push_back(y);
    }
    void Polygon::remove_last_point()
    {
        if(!x_list_.empty() && !y_list_.empty()){
            x_list_.pop_back();
            y_list_.pop_back();
        }
    }
    void Polygon::close(){
        is_closed_ = true;
    }
    bool Polygon::is_closed() const
    {
        return is_closed_;
    }
};