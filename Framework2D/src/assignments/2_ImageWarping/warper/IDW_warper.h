// HW2_TODO: Implement the IDWWarper class
#pragma once

#include "warper.h"
#include <vector>

using namespace std;
namespace USTC_CG
{
class IDWWarper : public Warper
{
   public:
    IDWWarper() = default;
    virtual ~IDWWarper() = default;
    // HW2_TODO: Implement the warp(...) function with IDW interpolation
    //继承，重载
    void warp(const unsigned char* src, unsigned char* dst, int width, int height);
    // HW2_TODO: other functions or variables if you need
    //从start_points_和end_points_(是vector，应该保留了本次操作所有的控制点对，对应p_i,q_i）中获取控制点对，计算每个像素点与控制点的距离，根据距离进行权重计算，最后根据权重对像素点进行插值计算
    void set_control_points(const vector<pair<float, float>>& start_points, const vector<pair<float, float>>& end_points){
        start_points_ = start_points;
        end_points_ = end_points;
    };
    //start_points_和end_points_是否可以直接用？
    //计算权重函数
    float sigma(int miu,int i,float x,float y);
        
    //计算权重函数
    float omiga(int miu,int i, float x, float y);
        
    //最小化E函数，求解线性方程组求解T_i（2 * 2矩阵）
    pair<pair<float,float>,pair<float,float>> determine_T(int i);
        
    //计算函数值
    pair<float,float> funtion_i(int i, float x, float y);
        
    //总函数f
    pair<int,int> function(float x,float y);

    

    private:
    vector<pair<float, float>> start_points_;
    vector<pair<float, float>> end_points_;
    vector<pair<pair<float,float>,pair<float,float>>> TMatrix;


};
}  // namespace USTC_CG