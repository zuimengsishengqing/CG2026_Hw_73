#pragma once

#include "common/image_widget.h"

namespace USTC_CG
{
// Image component for warping and other functions
class WarpingWidget : public ImageWidget
{
   public:
    explicit WarpingWidget(
        const std::string& label,
        const std::string& filename);
    virtual ~WarpingWidget() noexcept = default;

    void draw() override;

    // Simple edit functions
    void invert();
    void mirror(bool is_horizontal, bool is_vertical);
    void gray_scale();
    void warping();
    void restore();

    // Enumeration for supported warping types.
    // HW2_TODO: more warping types.
    enum WarpingType
    {
        kDefault = 0,
        kFisheye = 1,
        kIDW = 2,
        kRBF = 3,
        kNN = 4,
    };
    // Warping type setters.
    void set_default();
    void set_fisheye();
    void set_IDW();
    void set_RBF();
    void set_NN();

    // Point selecting interaction
    void enable_selecting(bool flag);
    void select_points();
    void init_selections();

    // Hole filling functions
    void enable_hole_filling(bool enable) { enable_hole_filling_ = enable; }
    void set_hole_filling_method(int method) { hole_filling_method_ = method; }
    void set_hole_search_radius(int radius) { hole_search_radius_ = radius; }
    bool is_hole_filling_enabled() const { return enable_hole_filling_; }
    int get_hole_filling_method() const { return hole_filling_method_; }
    int get_hole_search_radius() const { return hole_search_radius_; }

   private:
    // Store the original image data
    std::shared_ptr<Image> back_up_;
    // The selected point couples for image warping
    std::vector<ImVec2> start_points_, end_points_;

    ImVec2 start_, end_;
    bool flag_enable_selecting_points_ = false;
    bool draw_status_ = false;
    WarpingType warping_type_;
    
    // Hole filling parameters
    bool enable_hole_filling_ = false;
    int hole_filling_method_ = 0;  // 0: IDW, 1: Nearest Neighbor
    int hole_search_radius_ = 15;

   private:
    // A simple "fish-eye" warping function
    std::pair<int, int> fisheye_warping(int x, int y, int width, int height);
};

}  // namespace USTC_CG