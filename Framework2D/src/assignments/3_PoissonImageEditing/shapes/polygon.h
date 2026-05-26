#pragma once

#include "shape.h"

#include <vector>
#include <utility>

namespace USTC_CG
{

class Polygon : public Shape
{
   public:
    Polygon() = default;

    explicit Polygon(const std::vector<std::pair<float, float>>& vertices)
        : vertices_(vertices)
    {
    }

    virtual ~Polygon() = default;

    void draw(const Config& config) const override;
    void update(float x, float y) override;
    void add_control_point(float x, float y) override;

    std::vector<std::pair<int, int>> get_interior_pixels() const;

    const std::vector<std::pair<float, float>>& get_vertices() const
    {
        return vertices_;
    }

   private:
    struct Edge
    {
        int y_max;
        float x_curr;
        float inv_k;
    };

    std::vector<std::pair<float, float>> vertices_;
};

}  // namespace USTC_CG