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
        virtual ~Freehand() = default;

        void draw(const Config& config) const;
        void update(float x, float y);
        void add_control_point(float x, float y);
        //移除最后一个控制点
        //void remove_last_point();

        private:
        std::vector<float> x_list_, y_list_;

    };
}