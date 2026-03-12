#include "RBF_warper.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <limits>
#include <Eigen/Dense>


using namespace std;
using namespace Eigen;

namespace USTC_CG{

    RBFWarper::RBFWarper(){
        A = MatrixXf::Identity(2, 2);
        b = MatrixXf::Zero(2, 1);
    }
    
    void RBFWarper::warp(const unsigned char* src, unsigned char* dst, int width, int height){
        //初始化
        A = MatrixXf(2,2);
        alpha.resize(start_points_.size());
        for(size_t i = 0; i < start_points_.size(); i++){
            alpha[i] = MatrixXf(2,1);
        }
        b = MatrixXf(2,1);
        //基于插值条件求解
        if(start_points_.size() == 0){
            //A 取 I2, b取0
            A = MatrixXf::Identity(2,2);
            b = MatrixXf::Zero(2,1);
        }else if(start_points_.size() == 1){
            A = MatrixXf::Identity(2,2);
            b(0) = end_points_[0].first - start_points_[0].first;
            b(1) = end_points_[0].second - start_points_[0].second;
        }else if(start_points_.size() == 2){//2个点，仿射变换
            //平移加缩放
            float x1 = start_points_[0].first;
            float y1 = start_points_[0].second;
            float x2 = start_points_[1].first;
            float y2 = start_points_[1].second;
            float u1 = end_points_[0].first;
            float v1 = end_points_[0].second;
            float u2 = end_points_[1].first;
            float v2 = end_points_[1].second;
            
            // 构造矩阵 P = [[x1, y1, 1], [x2, y2, 1]]
            MatrixXf P(2, 3);
            P << x1, y1, 1,
                x2, y2, 1;
            
            // 构造矩阵 Q = [[u1, v1], [u2, v2]]
            MatrixXf Q(2, 2);
            Q << u1, v1,
                u2, v2;
            
            // 求解 P * X = Q，其中 X = [[a11, a21], [a12, a22], [b1, b2]]
            // 使用最小二乘法求解
            MatrixXf X = P.colPivHouseholderQr().solve(Q);
            
            // 提取A和b
            A = MatrixXf(2, 2);
            A << X(0, 0), X(1, 0),
                X(0, 1), X(1, 1);
            
            b = MatrixXf(2, 1);
            b << X(2, 0), X(2, 1);

        }else{//3个点或更多

            // 构造P(n * 3)
            MatrixXf P(start_points_.size(), 3);
            for(size_t i = 0; i < start_points_.size(); i++){
                P(i, 0) = start_points_[i].first;
                P(i, 1) = start_points_[i].second;
                P(i, 2) = 1;
            }
            // 构造Q(n * 2)
            MatrixXf Q(start_points_.size(), 2);
            for(size_t i = 0; i < start_points_.size(); i++){
                Q(i, 0) = end_points_[i].first;
                Q(i, 1) = end_points_[i].second;
            }
            // 求解 P * X = Q，其中 X = [[a11, a21], [a12, a22], [b1, b2]]
            // 使用最小二乘法求解
            MatrixXf X = P.colPivHouseholderQr().solve(Q);
            
            // 提取A和b
            A = MatrixXf(2, 2);
            A << X(0, 0), X(1, 0),
                X(0, 1), X(1, 1);
            
            b = MatrixXf(2, 1);
            b << X(2, 0), X(2, 1);  

            //求解alpha
            //构造G矩阵(n * n)
            MatrixXf G(start_points_.size(), start_points_.size());
            for(size_t i = 0; i < start_points_.size(); i++){
                for(size_t j = 0 ; j < start_points_.size(); j++){
                    G(i, j) = g(start_points_[i].first, start_points_[i].second, start_points_[j].first, start_points_[j].second, i);
                }
            }

            MatrixXf residual = Q - P * X;
            // 求解G* alpha = residual
            MatrixXf alpha_matrix = G.colPivHouseholderQr().solve(residual);
            //提取alpha并存入向量
            for(size_t i = 0; i < start_points_.size(); i++){
                alpha[i] = alpha_matrix.row(i);
            }
        }
        
        //把实际每个点代入计算，处理图片
        for(int y = 0; y < height; y++){
            for(int x = 0; x < width; x++){
                // 计算变换后的坐标
                auto [new_x, new_y] = function(x, y);
                
                // 检查坐标是否在图像范围内
                if(new_x >= 0 && new_x < width && new_y >= 0 && new_y < height){
                    // 获取原图像像素
                    int src_index = (y * width + x) * 4;  // RGBA格式
                    // 设置目标图像像素
                    int dst_index = (new_y * width + new_x) * 4;
                    dst[dst_index] = src[src_index];
                    dst[dst_index + 1] = src[src_index + 1];
                    dst[dst_index + 2] = src[src_index + 2];
                    dst[dst_index + 3] = src[src_index + 3];
                }
            }
        }

    };
    MatrixXf RBFWarper::A_p(float x,float y){
        MatrixXf p(2,1);
        p(0) = x;
        p(1) = y;
        MatrixXf ans(2,1);
        ans = A * p + b;//A*p+b
        return ans;
    }
    MatrixXf RBFWarper::R_p(float x,float y){
        MatrixXf ans = MatrixXf::Zero(2,1);
        for(size_t i = 0; i < start_points_.size(); i++){
            ans += alpha[i] * g(x,y, start_points_[i].first, start_points_[i].second, i);
        }
        return ans;
    }
    float RBFWarper::g(float x1, float y1, float x2, float y2, int i){//提供两个坐标值
        float d2 = pow(x1 - x2,2) + pow(y1 - y2,2);
        //r_i2: 计算控制点pi到其他所有控制点的最小距离的平方
        float ri2 = numeric_limits<float>::infinity();
        for(size_t j = 0; j < start_points_.size(); j++){
            if(j == i) continue;
            float dx = start_points_[i].first - start_points_[j].first;
            float dy = start_points_[i].second - start_points_[j].second;
            float dist2 = dx * dx + dy * dy;
            ri2 = min(ri2, dist2);
        }
        // ====   +/- 1/2次幂，可能穿调整   ====
        return sqrt(d2 + ri2);    
    }

    pair<int ,int> RBFWarper::function(float x,float y){
        MatrixXf ans_2(2,1);
        ans_2 = A_p(x,y) + R_p(x,y);
        return {static_cast<int>(std::round(ans_2(0))), static_cast<int>(std::round(ans_2(1)))};
    }

}