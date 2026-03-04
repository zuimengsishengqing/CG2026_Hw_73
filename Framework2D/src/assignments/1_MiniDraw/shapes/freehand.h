#pragma once

#include <vector>
#include "shape.h"
namespace USTC_CG
{
    class Freehand: public Shape
    {
        public:
        Freehand();
        Freehand(std::vector<float> x_list, std::vector<float> y_list);
    };
}