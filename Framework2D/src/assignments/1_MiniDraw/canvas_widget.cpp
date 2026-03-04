#include "canvas_widget.h"

#include <cmath>
#include <iostream>

#include "imgui.h"
#include "shapes/line.h"
#include "shapes/rect.h"
#include "shapes/ellipse.h"
#include "shapes/polygon.h"
#include "shapes/freehand.h"

namespace USTC_CG
{
void Canvas::draw()
{
    draw_background();
    // HW1_TODO: more interaction events
    if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        mouse_click_event();
    if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        mouse_right_click_event();
    mouse_move_event();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        mouse_release_event();

    draw_shapes();
}

void Canvas::set_attributes(const ImVec2& min, const ImVec2& size)
{
    canvas_min_ = min;
    canvas_size_ = size;
    canvas_minimal_size_ = size;
    canvas_max_ =
        ImVec2(canvas_min_.x + canvas_size_.x, canvas_min_.y + canvas_size_.y);
}

void Canvas::show_background(bool flag)
{
    show_background_ = flag;
}

void Canvas::set_default()
{
    draw_status_ = false;
    shape_type_ = kDefault;
}

void Canvas::set_line()
{
    draw_status_ = false;
    shape_type_ = kLine;
}

void Canvas::set_rect()
{
    draw_status_ = false;
    shape_type_ = kRect;
}

void Canvas::set_ellipse()
{
    draw_status_ = false;
    shape_type_ = kEllipse;
}

void Canvas::set_polygon()
{
    draw_status_ = false;
    shape_type_ = kPolygon;
}
void Canvas::set_freehand()
{
    draw_status_ = false;
    shape_type_ = kFreehand;
}
// HW1_TODO: more shape types, implements

void Canvas::clear_shape_list()
{
    shape_list_.clear();
}

void Canvas::draw_background()
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    if (show_background_)
    {
        // Draw background recrangle
        draw_list->AddRectFilled(canvas_min_, canvas_max_, background_color_);
        // Draw background border
        draw_list->AddRect(canvas_min_, canvas_max_, border_color_);
    }
    /// Invisible button over the canvas to capture mouse interactions.
    ImGui::SetCursorScreenPos(canvas_min_);
    ImGui::InvisibleButton(
        label_.c_str(), canvas_size_, ImGuiButtonFlags_MouseButtonLeft);
    // Record the current status of the invisible button
    is_hovered_ = ImGui::IsItemHovered();
    is_active_ = ImGui::IsItemActive();
}

void Canvas::draw_shapes()
{
    Shape::Config s = { .bias = { canvas_min_.x, canvas_min_.y } };
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // ClipRect can hide the drawing content outside of the rectangular area
    draw_list->PushClipRect(canvas_min_, canvas_max_, true);
    for (const auto& shape : shape_list_)
    {
        shape->draw(s);
    }
    if (draw_status_ && current_shape_)
    {
        current_shape_->draw(s);
    }
    draw_list->PopClipRect();
}

void Canvas::mouse_click_event()
{
    // HW1_TODO: Drawing rule for more primitives
    start_point_ = end_point_ = mouse_pos_in_canvas();
    // 单独处理自由绘制
    if(shape_type_ == kFreehand){
        //点击后开始在屏幕自由绘制
        if(!current_shape_){
            current_shape_ = std::make_shared<Freehand>();
            current_shape_->add_control_point(start_point_.x, start_point_.y);
            current_shape_->add_control_point(end_point_.x, end_point_.y);
        }
        draw_status_ = true;
        return;
    }
    // 处理多边形绘制的特殊情况
    if (shape_type_ == kPolygon)
    {
        if (!current_shape_)
        {
            // 首次创建多边形
            current_shape_ = std::make_shared<Polygon>();
            current_shape_->add_control_point(start_point_.x, start_point_.y);
            current_shape_->add_control_point(end_point_.x, end_point_.y);
        }
        else
        {
            // 添加新的顶点
            auto polygon = std::dynamic_pointer_cast<Polygon>(current_shape_);
            if (polygon)
            {
                // 移除临时点（最后一个点）
                polygon->remove_last_point();
                // 添加新的固定点
                polygon->add_control_point(start_point_.x, start_point_.y);
                // 添加新的临时点
                polygon->add_control_point(end_point_.x, end_point_.y);
            }
        }
        draw_status_ = true;
        return;
    }
    
    // 处理其他形状的绘制
    if (!draw_status_)
    {
        draw_status_ = true;
        switch (shape_type_)
        {
            case USTC_CG::Canvas::kDefault:
            {
                break;
            }
            case USTC_CG::Canvas::kLine:
            {
                current_shape_ = std::make_shared<Line>(
                    start_point_.x, start_point_.y, end_point_.x, end_point_.y);
                break;
            }
            case USTC_CG::Canvas::kRect:
            {
                current_shape_ = std::make_shared<Rect>(
                    start_point_.x, start_point_.y, end_point_.x, end_point_.y);
                break;
            }
            // HW1_TODO: case USTC_CG::Canvas::kEllipse:
            case USTC_CG::Canvas::kEllipse:
            {
                current_shape_ = std::make_shared<Ellipse>(
                    start_point_.x, start_point_.y, end_point_.x, end_point_.y);
                break;
            }
            default: break;
        }
    }
    else
    {
        // 结束绘制非多边形形状
        draw_status_ = false;
        if (current_shape_)
        {
            shape_list_.push_back(current_shape_);
            current_shape_.reset();
        }
    }
}

void Canvas::mouse_move_event()
{
    // HW1_TODO: Drawing rule for more primitives
    if (draw_status_)
    {
        end_point_ = mouse_pos_in_canvas();
        if (current_shape_)
        {
            current_shape_->update(end_point_.x, end_point_.y);
            //自由绘制，每帧更新
            if(shape_type_ == kFreehand){
                current_shape_->add_control_point(end_point_.x, end_point_.y);
            }
        }
    }
}

void Canvas::mouse_release_event()
{
    // HW1_TODO: Drawing rule for more primitives
    //自由绘制，松开鼠标停止落笔：
    if(shape_type_ == kFreehand && current_shape_ && draw_status_){
        draw_status_ = false;
        shape_list_.push_back(current_shape_);
        current_shape_.reset();
    }
}
void Canvas::mouse_right_click_event(){
    if(shape_type_ == kPolygon && current_shape_){
        auto polygon = std::dynamic_pointer_cast<Polygon>(current_shape_);
        if(polygon){
            //关闭,状态转化和vector清空
            polygon->close();
            shape_list_.push_back(current_shape_);
            current_shape_.reset();
            draw_status_ = false;
            
        }
    }
}

ImVec2 Canvas::mouse_pos_in_canvas() const
{
    ImGuiIO& io = ImGui::GetIO();
    const ImVec2 mouse_pos_in_canvas(
        io.MousePos.x - canvas_min_.x, io.MousePos.y - canvas_min_.y);
    return mouse_pos_in_canvas;
}
}  // namespace USTC_CG