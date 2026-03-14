#pragma once

#include <memory>

#include "common/window.h"
#include "warping_widget.h"

namespace USTC_CG
{
class ImageWarping : public Window
{
   public:
    explicit ImageWarping(const std::string& window_name);
    ~ImageWarping();

    void draw();

   private:
    void draw_toolbar();
    void draw_image();
    void draw_open_image_file_dialog();
    void draw_save_image_file_dialog();
    void draw_hole_filling_options();

    std::shared_ptr<WarpingWidget> p_image_ = nullptr;

    bool flag_show_main_view_ = true;
    bool flag_open_file_dialog_ = false;
    bool flag_save_file_dialog_ = false;
    
    // Hole filling settings
    bool enable_hole_filling_ = false;
    int hole_filling_method_ = 0;  // 0: IDW, 1: Nearest Neighbor
    int hole_search_radius_ = 15;
    bool show_hole_filling_options_ = false;
};
}  // namespace USTC_CG