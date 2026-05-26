#pragma once

#include <vector>
#include <utility>
#include <string>
#include <tuple>

#ifdef USE_ANNOY
#include "annoylib.h"
#endif

namespace USTC_CG
{

class HoleFiller
{
public:
    HoleFiller();
    ~HoleFiller();

    void fill_holes(unsigned char* image, int width, int height, 
                   int search_radius = 10, int method = 0);

    void set_search_radius(int radius) { search_radius_ = radius; }
    void set_method(int method) { method_ = method; }

private:
    int search_radius_;
    int method_;

    bool is_hole_pixel(const unsigned char* image, int width, int height, int x, int y);
    
    std::vector<std::pair<int, int>> find_known_pixels(
        const unsigned char* image, int width, int height, 
        int x, int y, int radius);

    std::vector<std::pair<int, int>> find_known_pixels_annoy(
        const unsigned char* image, int width, int height, 
        int x, int y, int radius);

    std::tuple<unsigned char, unsigned char, unsigned char> interpolate_color(
        const unsigned char* image, int width, int height,
        const std::vector<std::pair<int, int>>& known_pixels,
        int target_x, int target_y);

    float calculate_distance(int x1, int y1, int x2, int y2);
    float calculate_idw_weight(float distance, float power = 2.0f);
};

}