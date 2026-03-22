#include "source_image_widget.h"

#include <algorithm>
#include <cmath>

namespace USTC_CG
{
using uchar = unsigned char;

SourceImageWidget::SourceImageWidget(
    const std::string& label,
    const std::string& filename)
    : ImageWidget(label, filename)
{
    if (data_)
        selected_region_mask_ =
            std::make_shared<Image>(data_->width(), data_->height(), 1);
}

void SourceImageWidget::draw()
{
    // Draw the image
    ImageWidget::draw();
    // Draw selected region
    if (flag_enable_selecting_region_)
        select_region();
}

void SourceImageWidget::enable_selecting(bool flag)
{
    flag_enable_selecting_region_ = flag;
}

void SourceImageWidget::set_region_type(RegionType type)
{
    region_type_ = type;
    draw_status_ = false;
    selected_shape_.reset();
    polygon_vertices_.clear();
    if (selected_region_mask_)
    {
        for (int i = 0; i < selected_region_mask_->width(); ++i)
            for (int j = 0; j < selected_region_mask_->height(); ++j)
                selected_region_mask_->set_pixel(i, j, { 0 });
    }
}

SourceImageWidget::RegionType SourceImageWidget::get_region_type() const
{
    return region_type_;
}

void SourceImageWidget::select_region()
{
    /// Invisible button over the canvas to capture mouse interactions.
    ImGui::SetCursorScreenPos(position_);
    ImGui::InvisibleButton(
        label_.c_str(),
        ImVec2(
            static_cast<float>(image_width_),
            static_cast<float>(image_height_)),
        ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    // Record the current status of the invisible button
    bool is_hovered_ = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();
    // Mouse events
    if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        mouse_click_event();
    }
    if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        finish_polygon_drawing();
    }
    mouse_move_event();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        mouse_release_event();

    // Region Shape Visualization
    if (selected_shape_)
    {
        Shape::Config s = { .bias = { position_.x, position_.y },
                            .line_color = { 255, 0, 0, 255 },
                            .line_thickness = 2.0f };
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        selected_shape_->draw(s);
    }  
}

std::shared_ptr<Image> SourceImageWidget::get_region_mask()
{
    return selected_region_mask_;
}

std::shared_ptr<Image> SourceImageWidget::get_data()
{
    return data_;
}

ImVec2 SourceImageWidget::get_position() const
{
    return start_;
}

std::tuple<int, int, int, int> SourceImageWidget::get_bounding_box() const
{
    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    
    if (region_type_ == kRect && selected_shape_)
    {
        // 矩形模式：使用矩形的边界
        min_x = static_cast<int>(std::min(start_.x, end_.x));
        min_y = static_cast<int>(std::min(start_.y, end_.y));
        max_x = static_cast<int>(std::max(start_.x, end_.x));
        max_y = static_cast<int>(std::max(start_.y, end_.y));
    }
    else if (region_type_ == kPolygon && selected_shape_)
    {
        // 多边形模式：计算顶点的边界框
        if (polygon_vertices_.empty())
        {
            return std::make_tuple(0, 0, 0, 0);
        }
        
        min_x = static_cast<int>(std::floor(polygon_vertices_[0].first));
        min_y = static_cast<int>(std::floor(polygon_vertices_[0].second));
        max_x = static_cast<int>(std::ceil(polygon_vertices_[0].first));
        max_y = static_cast<int>(std::ceil(polygon_vertices_[0].second));
        
        for (const auto& vertex : polygon_vertices_)
        {
            int x = static_cast<int>(std::floor(vertex.first));
            int y = static_cast<int>(std::floor(vertex.second));
            min_x = std::min(min_x, x);
            min_y = std::min(min_y, y);
            max_x = std::max(max_x, x);
            max_y = std::max(max_y, y);
        }
    }
    else
    {
        // 默认情况：整个图像
        min_x = 0;
        min_y = 0;
        max_x = image_width_ - 1;
        max_y = image_height_ - 1;
    }
    
    // 确保边界在图像范围内
    min_x = std::max(0, min_x);
    min_y = std::max(0, min_y);
    max_x = std::min(image_width_ - 1, max_x);
    max_y = std::min(image_height_ - 1, max_y);
    
    return std::make_tuple(min_x, min_y, max_x, max_y);
}

void SourceImageWidget::mouse_click_event()
{
    // Start drawing the region 
    if (!draw_status_)
    {
        draw_status_ = true;
        start_ = end_ = mouse_pos_in_canvas();
        // HW3_TODO(optional): You can add more shapes for region selection. You
        // can also consider using the implementation in HW1. (We use rectangle
        // for example)
        switch (region_type_)
        {
            case USTC_CG::SourceImageWidget::kDefault: break;
            case USTC_CG::SourceImageWidget::kRect:
            {
                selected_shape_ =
                    std::make_unique<Rect>(start_.x, start_.y, end_.x, end_.y);
                break;
            }
            case USTC_CG::SourceImageWidget::kPolygon:
            {
                polygon_vertices_.clear();
                polygon_vertices_.push_back({ start_.x, start_.y });
                selected_shape_ = std::make_unique<Polygon>(polygon_vertices_);
                break;
            }
            default: break;
        }
    }
    else
    {
        if (region_type_ == kPolygon && selected_shape_)
        {
            ImVec2 pos = mouse_pos_in_canvas();
            polygon_vertices_.push_back({ pos.x, pos.y });
            auto* polygon = static_cast<Polygon*>(selected_shape_.get());
            polygon->add_control_point(pos.x, pos.y);
        }
    }
}

void SourceImageWidget::mouse_move_event()
{
    // Keep updating the region
    if (draw_status_)
    {
        end_ = mouse_pos_in_canvas();
        if (selected_shape_)
            selected_shape_->update(end_.x, end_.y);
    }
}

void SourceImageWidget::mouse_release_event()
{
    // Finish drawing the region
    if (draw_status_ && selected_shape_ && region_type_ == kRect)
    {
        draw_status_ = false;
        // Update the selected region.
        update_selected_region();
    }
}

ImVec2 SourceImageWidget::mouse_pos_in_canvas() const
{
    ImGuiIO& io = ImGui::GetIO();
    // The position should not be out of the canvas
    const ImVec2 mouse_pos_in_canvas(
        std::clamp<float>(io.MousePos.x - position_.x, 0, (float)image_width_),
        std::clamp<float>(
            io.MousePos.y - position_.y, 0, (float)image_height_));
    return mouse_pos_in_canvas;
}

void SourceImageWidget::update_selected_region()
{
    if (selected_shape_ == nullptr)
        return;
    // HW3_TODO(Optional): The selected_shape_ call its get_interior_pixels()
    // function to get the interior pixels. For other shapes, you can implement
    // their own get_interior_pixels()
    std::vector<std::pair<int, int>> interior_pixels =
        selected_shape_->get_interior_pixels();
    // Clear the selected region mask
    for (int i = 0; i < selected_region_mask_->width(); ++i)
        for (int j = 0; j < selected_region_mask_->height(); ++j)
            selected_region_mask_->set_pixel(i, j, { 0 });
    // Set the selected pixels with 255
    for (const auto& pixel : interior_pixels)
    {
        int x = pixel.first;
        int y = pixel.second;
        if (x < 0 || x >= selected_region_mask_->width() || 
            y < 0 || y >= selected_region_mask_->height())
            continue;
        selected_region_mask_->set_pixel(x, y, { 255 });
    }
}

void SourceImageWidget::finish_polygon_drawing()
{
    if (region_type_ == kPolygon && draw_status_)
    {
        draw_status_ = false;
        update_selected_region();
    }
}
}  // namespace USTC_CG