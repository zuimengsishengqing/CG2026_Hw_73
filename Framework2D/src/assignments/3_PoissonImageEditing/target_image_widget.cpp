#include "target_image_widget.h"
#include "seamless_clone.h"
#include <cmath>

namespace USTC_CG
{
using uchar = unsigned char;

TargetImageWidget::TargetImageWidget(
    const std::string& label,
    const std::string& filename)
    : ImageWidget(label, filename)
{
    if (data_)
        back_up_ = std::make_shared<Image>(*data_);
}

void TargetImageWidget::draw()
{
    // Draw the image
    ImageWidget::draw();
    // Invisible button for interactions
    ImGui::SetCursorScreenPos(position_);
    ImGui::InvisibleButton(
        label_.c_str(),
        ImVec2(
            static_cast<float>(image_width_),
            static_cast<float>(image_height_)),
        ImGuiButtonFlags_MouseButtonLeft);
    bool is_hovered_ = ImGui::IsItemHovered();
    // When the mouse is clicked or moving, we would adapt clone function to
    // copy the selected region to the target.

    if (is_hovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        mouse_click_event();
    }
    mouse_move_event();
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        mouse_release_event();
    }
}

void TargetImageWidget::set_source(std::shared_ptr<SourceImageWidget> source)
{
    source_image_ = source;
}

void TargetImageWidget::set_realtime(bool flag)
{
    flag_realtime_updating = flag;
}

void TargetImageWidget::restore()
{
    *data_ = *back_up_;
    update();
}

void TargetImageWidget::set_paste()
{
    clone_type_ = kPaste;
    // 切换到paste模式时，重置预分解状态
    is_seamless_precomputed_ = false;
    seamless_clone_.reset();
}

void TargetImageWidget::set_seamless()
{
    clone_type_ = kSeamless;
    // 切换到seamless模式时，重置预分解状态
    is_seamless_precomputed_ = false;
    seamless_clone_.reset();
}

void TargetImageWidget::set_mixed_seamless()
{
    clone_type_ = kMixedSeamless;
    // 切换到mixed seamless模式时，重置预分解状态
    is_seamless_precomputed_ = false;
    seamless_clone_.reset();
}

void TargetImageWidget::clone()
{
    // The implementation of different types of cloning
    // HW3_TODO: 
    // 1. In this function, you should at least implement the "seamless"
    // cloning labeled by `clone_type_ ==kSeamless`.
    //
    // 2. It is required to improve the efficiency of your seamless cloning to
    // achieve real-time editing. (Use decomposition of sparse matrix before
    // solve the linear system). The real-time updating (update when the mouse
    // is moving) is only available when the checkerboard is selected. 
    if (data_ == nullptr || source_image_ == nullptr ||
        source_image_->get_region_mask() == nullptr)
        return;
    // The selected region in the source image, this would be a binary mask.
    // The **size** of the mask should be the same as the source image.
    // The **value** of the mask should be 0 or 255: 0 for the background and
    // 255 for the selected region.
    std::shared_ptr<Image> mask = source_image_->get_region_mask();  //SourceImageWidget::selected_region_mask_存储了一张与源图像一样尺寸的黑白图像
    //，用 source_image_->get_region_mask() 获取，它的每一个像素只可能有两个值（0 or 255），标记为 255 的像素表示选中的区域；

    switch (clone_type_)
    {
        case USTC_CG::TargetImageWidget::kDefault: break;
        case USTC_CG::TargetImageWidget::kPaste:
        {
            restore();

            for (int x = 0; x < mask->width(); ++x)
            {
                for (int y = 0; y < mask->height(); ++y)
                {
                    int tar_x =
                        static_cast<int>(mouse_position_.x) + x -
                        static_cast<int>(source_image_->get_position().x); //记录了在源图像中，选定区域的相对位置（例如矩形的左上角）；
                    int tar_y =
                        static_cast<int>(mouse_position_.y) + y -
                        static_cast<int>(source_image_->get_position().y);
                    if (0 <= tar_x && tar_x < image_width_ && 0 <= tar_y &&
                        tar_y < image_height_ && mask->get_pixel(x, y)[0] > 0)
                    {
                        data_->set_pixel(
                            tar_x,
                            tar_y,
                            source_image_->get_data()->get_pixel(x, y));
                    }
                }
            }
            break;
        }
        case USTC_CG::TargetImageWidget::kSeamless:
        {
            restore();
            
            // 计算偏移量：选中区域在目标图像中的位置
            int offset_x = static_cast<int>(mouse_position_.x) - 
                          static_cast<int>(source_image_->get_position().x);
            int offset_y = static_cast<int>(mouse_position_.y) - 
                          static_cast<int>(source_image_->get_position().y);
            
            // 计算选中区域在源图像中的位置（左上角）
            int origin_x = static_cast<int>(source_image_->get_position().x);
            int origin_y = static_cast<int>(source_image_->get_position().y);
            
            // 检查是否需要重新创建SeamlessClone对象
            if(!seamless_clone_ || !is_seamless_precomputed_){
                // 创建SeamlessClone对象
                seamless_clone_ = std::make_shared<SeamlessClone>(
                    source_image_->get_data(),
                    data_,
                    mask,
                    offset_x,
                    offset_y,
                    origin_x,
                    origin_y
                );
                
                // 设置混合梯度标志为false
                seamless_clone_->set_mixed_gradient(false);
                
                // 预分解矩阵A
                seamless_clone_->precompute();
                is_seamless_precomputed_ = true;
            }
            
            // 更新偏移量
            seamless_clone_->update_offset(offset_x, offset_y);
            
            // 使用快速求解方法
            std::shared_ptr<Image> result;
            if(flag_realtime_updating){
                // 实时编辑：使用预分解的快速求解
                result = seamless_clone_->solve_fast();
            }else{
                // 非实时编辑：使用普通求解方法
                result = seamless_clone_->solve();
            }
            
            // 将结果应用到目标图像
            if (result)
            {
                *data_ = *result;
            }
            
            break;
        }
        case USTC_CG::TargetImageWidget::kMixedSeamless:
        {
            restore();
            
            // 计算偏移量：选中区域在目标图像中的位置
            int offset_x = static_cast<int>(mouse_position_.x) - 
                          static_cast<int>(source_image_->get_position().x);
            int offset_y = static_cast<int>(mouse_position_.y) - 
                          static_cast<int>(source_image_->get_position().y);
            
            // 计算选中区域在源图像中的位置（左上角）
            int origin_x = static_cast<int>(source_image_->get_position().x);
            int origin_y = static_cast<int>(source_image_->get_position().y);
            
            // 检查是否需要重新创建SeamlessClone对象
            if(!seamless_clone_ || !is_seamless_precomputed_){
                // 创建SeamlessClone对象
                seamless_clone_ = std::make_shared<SeamlessClone>(
                    source_image_->get_data(),
                    data_,
                    mask,
                    offset_x,
                    offset_y,
                    origin_x,
                    origin_y
                );
                
                // 设置混合梯度标志为true
                seamless_clone_->set_mixed_gradient(true);
                
                // 预分解矩阵A
                seamless_clone_->precompute();
                is_seamless_precomputed_ = true;
            }
            
            // 更新偏移量（混合梯度模式下，每次拖动都需要用最新offset重新计算B）
            seamless_clone_->update_offset(offset_x, offset_y);
            
            // 使用快速求解方法
            std::shared_ptr<Image> result;
            if(flag_realtime_updating){
                // 实时编辑：使用预分解的快速求解
                // 注意：混合梯度模式下，solve_fast会使用最新offset重新计算B向量
                result = seamless_clone_->solve_fast();
            }else{
                // 非实时编辑：使用普通求解方法
                result = seamless_clone_->solve();
            }
            
            // 将结果应用到目标图像
            if (result)
            {
                *data_ = *result;
            }
            
            break;
        }
        default: break;
    }

    update();
}

void TargetImageWidget::mouse_click_event()
{
    edit_status_ = true;
    mouse_position_ = mouse_pos_in_canvas();
    
    // 在新的点击时重置预分解状态，因为用户可能选择了不同的区域
    if(clone_type_ == kSeamless || clone_type_ == kMixedSeamless){
        is_seamless_precomputed_ = false;
        seamless_clone_.reset();
    }
    
    clone();
}

void TargetImageWidget::mouse_move_event()
{
    if (edit_status_)
    {
        mouse_position_ = mouse_pos_in_canvas();
        if (flag_realtime_updating)
            clone();
    }
}

void TargetImageWidget::mouse_release_event()
{
    if (edit_status_)
    {
        edit_status_ = false;
    }
}

ImVec2 TargetImageWidget::mouse_pos_in_canvas() const
{
    ImGuiIO& io = ImGui::GetIO();
    return ImVec2(io.MousePos.x - position_.x, io.MousePos.y - position_.y);
}
}  // namespace USTC_CG