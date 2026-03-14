#pragma once

#include <vector>
#include "shape.h"
namespace USTC_CG
{
    class Polygon: public Shape
    {
        public:
        Polygon();
        Polygon(std::vector<float> x_list, std::vector<float> y_list);
        virtual ~Polygon() = default;

        void draw(const Config& config) const;
        void update(float x, float y);
        void add_control_point(float x, float y);
        //移除最后一个控制点（通常是临时点）
        void remove_last_point();
        //连接最后一条边，补齐
        void close();


        bool is_closed() const;
        private:
        std::vector<float> x_list_, y_list_;
        bool is_closed_ = false;
    };
};