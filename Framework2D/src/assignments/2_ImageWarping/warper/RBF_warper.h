// HW2_TODO: Implement the RBFWarper class
#pragma once

#include "warper.h"
#include <vector>
#include <Eigen/Dense>
using namespace std;
using namespace Eigen;

namespace USTC_CG
{
class RBFWarper : public Warper
{
   public:
    RBFWarper();
    virtual ~RBFWarper() = default;
    // HW2_TODO: Implement the warp(...) function with RBF interpolation
    void warp(const unsigned char* src, unsigned char* dst, int width, int height);
    // HW2_TODO: other functions or variables if you need
    void set_control_points(const vector<pair<float, float>>& start_points, const vector<pair<float, float>>& end_points){
        start_points_ = start_points;
        end_points_ = end_points;
    };
    MatrixXf A_p(float x,float y);
    MatrixXf R_p(float x,float y);
    float g(float x1, float y1, float x2, float y2, int i);
    //总函数f
    pair<int,int> function(float x,float y);

    private:
    vector<pair<float, float>> start_points_;
    vector<pair<float, float>> end_points_;

    MatrixXf A;
    MatrixXf b;
    vector<MatrixXf> alpha;
    vector<float> ri;  // 存储每个控制点的ri值


};
}  // namespace USTC_CG