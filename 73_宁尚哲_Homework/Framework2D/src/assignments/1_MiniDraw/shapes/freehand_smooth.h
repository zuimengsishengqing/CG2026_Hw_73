#pragma once

#include <vector>
#include "shape.h"

namespace USTC_CG
{
    class FreehandSmooth : public Shape
    {
    public:
        FreehandSmooth();
        FreehandSmooth(std::vector<float> x_list, std::vector<float> y_list);
        virtual ~FreehandSmooth() = default;

        void draw(const Config& config) const;
        void update(float x, float y);
        void add_control_point(float x, float y);
        void force_add_control_point(float x, float y);
        
    private:
        // Smooth drawing method
        void draw_smooth(const Config& config) const;
        // Calculate distance between two points
        float distance(float x1, float y1, float x2, float y2) const;
        
        std::vector<float> x_list_, y_list_;
        // Minimum sampling distance
        static constexpr float MIN_SAMPLE_DISTANCE = 2.0f;
    };
}