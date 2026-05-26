#include "IDW_warper.h"
#include "hole_filler.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <vector>
#include <Eigen/Dense>

namespace USTC_CG
{
    void IDWWarper::warp(const unsigned char* src, unsigned char* dst, int width, int height)
    {
        //Implement the warp(...) function with IDW interpolation
        //dst是src经过变换后的图像
        //src是原图像
        //遍历每个像素点，计算其在原图像中的位置，并根据控制点进行插值计算
        
        if (start_points_.empty() || end_points_.empty())
            return;
            
        //遍历每个像素点,调用function函数计算每个像素点新的位置：
        for(int y = 0; y < height; y++){
            for(int x = 0; x < width; x++){
                // 计算变换后的坐标
                pair<float,float> new_pos = function(x, y);
                
                // 将float坐标转换为int坐标（四舍五入）
                int new_x = static_cast<int>(std::round(new_pos.first));
                int new_y = static_cast<int>(std::round(new_pos.second));
                
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
        
        // 如果启用了空洞填充，则填充空洞
        if(fill_holes_){
            cout<<"Starting hole filling..."<<endl;
            HoleFiller filler;
            filler.set_search_radius(hole_search_radius_);
            filler.set_method(hole_filling_method_);
            filler.fill_holes(dst, width, height, hole_search_radius_, hole_filling_method_);
        }
    }
    
    float IDWWarper::sigma(int miu,int i,float x,float y){
        //计算权重函数
        float dx = x - start_points_[i].first;
        float dy = y - start_points_[i].second;
        float distance = std::pow((std::pow(dx,miu) + std::pow(dy,miu)), 1.0/miu);
        
        if (distance < 1e-10f)  // 避免除零
            return 1e10f;
            
        return 1.0f / std::pow(distance, miu);
    }
    
    float IDWWarper::omiga(int miu, int i, float x, float y){
        if(start_points_[i].first == x && start_points_[i].second == y)
            return 1.0f;
        else
        {
            //计算权重函数
            float sum_sigma = 0;
            for(size_t j = 0; j < start_points_.size(); j++){
                sum_sigma += sigma(miu, j, x, y);
            }
            
            if (sum_sigma < 1e-10f)  // 避免除零
                return 0.0f;
                
            return sigma(miu, i, x, y) / sum_sigma;
        }
    }
    pair<pair<float,float>,pair<float,float>> IDWWarper::determine_T(int i){
        //最小化E函数，求解线性方程组求解T_i（2 * 2矩阵）
        // 根据公式：Ti * A = B
        // 其中 A = Σ (pj - pi) * (pj - pi)^T
        //       B = Σ (qj - qi) * (pj - pi)^T
        
        if (start_points_.empty() || end_points_.empty()) {
            return {{1.0f, 0.0f}, {0.0f, 1.0f}}; // 返回单位矩阵
        }
        
        // 获取pi和qi
        float pi_x = start_points_[i].first;
        float pi_y = start_points_[i].second;
        float qi_x = end_points_[i].first;
        float qi_y = end_points_[i].second;
        
        // 初始化A和B矩阵
        Eigen::Matrix2d A = Eigen::Matrix2d::Zero();
        Eigen::Matrix2d B = Eigen::Matrix2d::Zero();
        
        // 遍历所有控制点，计算A和B
        for (size_t j = 0; j < start_points_.size(); j++) {
            if (j == i) continue; // 跳过i本身
            
            // 获取pj和qj
            float pj_x = start_points_[j].first;
            float pj_y = start_points_[j].second;
            float qj_x = end_points_[j].first;
            float qj_y = end_points_[j].second;
            
            // 计算pj - pi和qj - qi
            float pj_minus_pi_x = pj_x - pi_x;
            float pj_minus_pi_y = pj_y - pi_y;
            float qj_minus_qi_x = qj_x - qi_x;
            float qj_minus_qi_y = qj_y - qi_y;
            
            // 计算外积 (pj - pi) * (pj - pi)^T
            Eigen::Vector2d pj_minus_pi(pj_minus_pi_x, pj_minus_pi_y);
            A += pj_minus_pi * pj_minus_pi.transpose();
            
            // 计算外积 (qj - qi) * (pj - pi)^T
            Eigen::Vector2d qj_minus_qi(qj_minus_qi_x, qj_minus_qi_y);
            B += qj_minus_qi * pj_minus_pi.transpose();
        }
        
        // 求解线性方程组 Ti * A = B
        Eigen::Matrix2d Ti;
        
        // 检查A是否可逆
        if (A.determinant() < 1e-10) {
            // A不可逆，返回单位矩阵
            Ti = Eigen::Matrix2d::Identity();
        } else {
            // 使用Eigen的solve方法求解
            Ti = B * A.inverse();
        }
        
        // 将结果转换为pair格式
        return {{static_cast<float>(Ti(0, 0)), static_cast<float>(Ti(0, 1))},
                {static_cast<float>(Ti(1, 0)), static_cast<float>(Ti(1, 1))}};
    }
    pair<float,float> IDWWarper::funtion_i(int i, float x, float y){
        //计算函数值
        //先计算T
        Eigen::Vector2d p(x - start_points_[i].first,y - start_points_[i].second);
        pair<float,float> ans = {determine_T(i).first.first * p(0) + determine_T(i).first.second * p(1) + end_points_[i].first,
                                determine_T(i).second.first * p(0) + determine_T(i).second.second * p(1) + end_points_[i].second};
        return ans;
    }
    pair<int ,int> IDWWarper::function(float x,float y){
        pair<float,float> ans_f = {0,0};
        for(int j = 0;j < start_points_.size();j++){
            ans_f.first += omiga(2,j,x,y) * funtion_i(j,x,y).first;
            ans_f.second += omiga(2,j,x,y) * funtion_i(j,x,y).second;
        
        }
        //将ans四舍五入
        pair<int,int> ans = {static_cast<int>(std::round(ans_f.first)),static_cast<int>(std::round(ans_f.second))};
        return ans;
    }
}