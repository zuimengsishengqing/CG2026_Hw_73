#pragma once

#include "polygon.h"

namespace USTC_CG
{

class Freehand : public Polygon
{
   public:
    Freehand() = default;

    explicit Freehand(const std::vector<std::pair<float, float>>& vertices)
        : Polygon(vertices)
    {
    }

    virtual ~Freehand() = default;

    void draw(const Config& config) const override;
    void update(float x, float y) override;
};

}  // namespace USTC_CG