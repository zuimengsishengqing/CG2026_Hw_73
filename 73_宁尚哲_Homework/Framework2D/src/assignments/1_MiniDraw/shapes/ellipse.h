#pragma once

#include "shape.h"
namespace USTC_CG
{
class Ellipse : public Shape
{
    public:
    Ellipse() = default;

    // Constructor to initialize an ellipse with start and end coordinates
    Ellipse(float start_point_x, float start_point_y, float end_point_x, float end_point_y)
        : start_point_x_(start_point_x),
          start_point_y_(start_point_y),
          end_point_x_(end_point_x),
          end_point_y_(end_point_y)
    {
    }

    virtual ~Ellipse() = default;


    // Draws the ellipse on the screen
    void draw(const Config& config) const override;

    // Overrides Shape's update function to adjust the ellipse size during
    // interaction
    void update(float x, float y) override;

   private:
    // Coordinates of the top-left and bottom-right corners of the rectangle
    float start_point_x_ = 0.0f, start_point_y_ = 0.0f;
    float end_point_x_ = 0.0f, end_point_y_ = 0.0f;
};
}
