#include "seamless_clone.h"

#include <iostream>
#include <Eigen/Sparse>

namespace USTC_CG{
//构造函数
SeamlessClone::SeamlessClone(std::shared_ptr<Image> src_img, std::shared_ptr<Image> tar_img, std::shared_ptr<Image> src_selected_mask, int offset_x, int offset_y,int origin_x, int origin_y){
    // 保存参数到成员变量
    src_img_ = src_img;
    tar_img_ = tar_img;
    src_selected_mask_ = src_selected_mask;
    offset_x_ = offset_x;
    offset_y_ = offset_y;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    //求解区域大小：应该是src_selected_mask
    W = src_selected_mask->width(); //移动图像，源图像和目标图像块的大小相同？
    H = src_selected_mask->height();
    //A* x = B,矩阵维度是图像所有像素点
    A = Eigen::SparseMatrix<double>(W * H, W * H); // 稀疏矩阵 A,系数矩阵（成员变量）
    B = Eigen::VectorXd(W * H); // 向量 B,右侧向量（成员变量）
    triplet_list.clear(); // 清空三元组列表
}
//填写 (x, y) 对应的方程系数
void SeamlessClone::fill_coefficient(int x,int y,int rgb_index){
    int idx = y * W + x;
    
    int mask_idx = (y * W + x) * src_selected_mask_->channels();
    if(src_selected_mask_->data()[mask_idx] == 0){
        return;
    }
    
    auto [gx, gy, gz] = g(x, y);
    auto [gx_up, gy_up, gz_up] = g(x, y - 1);
    auto [gx_down, gy_down, gz_down] = g(x, y + 1);
    auto [gx_left, gy_left, gz_left] = g(x - 1, y);
    auto [gx_right, gy_right, gz_right] = g(x + 1, y);
    
    double g_center = (rgb_index == 0) ? gx : (rgb_index == 1) ? gy : gz;
    double g_up = (rgb_index == 0) ? gx_up : (rgb_index == 1) ? gy_up : gz_up;
    double g_down = (rgb_index == 0) ? gx_down : (rgb_index == 1) ? gy_down : gz_down;
    double g_left = (rgb_index == 0) ? gx_left : (rgb_index == 1) ? gy_left : gz_left;
    double g_right = (rgb_index == 0) ? gx_right : (rgb_index == 1) ? gy_right : gz_right;
    
    double gradient_rhs = 4.0 * g_center - g_up - g_down - g_left - g_right;
    
    switch(point_type(x,y)){
        case IN:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 4.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y - 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y + 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x - 1), -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x + 1), -1.0));
            B(idx) = gradient_rhs;
            break;
        }
        case L:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 3.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y - 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y + 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x + 1), -1.0));
            B(idx) = gradient_rhs + f(x - 1, y, rgb_index);
            break;
        }
        case R:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 3.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y - 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y + 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x - 1), -1.0));
            B(idx) = gradient_rhs + f(x + 1, y, rgb_index);
            break;
        }
        case U:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 3.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y + 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x - 1), -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x + 1), -1.0));
            B(idx) = gradient_rhs + f(x, y - 1, rgb_index);
            break;
        }
        case D:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 3.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y - 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x - 1), -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x + 1), -1.0));
            B(idx) = gradient_rhs + f(x, y + 1, rgb_index);
            break;
        }
        case LU:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 2.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y + 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x + 1), -1.0));
            B(idx) = gradient_rhs + f(x - 1, y, rgb_index) + f(x, y - 1, rgb_index);
            break;
        }
        case RU:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 2.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y + 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x - 1), -1.0));
            B(idx) = gradient_rhs + f(x + 1, y, rgb_index) + f(x, y - 1, rgb_index);
            break;
        }
        case LD:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 2.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y - 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x + 1), -1.0));
            B(idx) = gradient_rhs + f(x - 1, y, rgb_index) + f(x, y + 1, rgb_index);
            break;
        }
        case RD:
        {
            triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 2.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, (y - 1) * W + x, -1.0));
            triplet_list.push_back(Eigen::Triplet<double>(idx, y * W + (x - 1), -1.0));
            B(idx) = gradient_rhs + f(x + 1, y, rgb_index) + f(x, y + 1, rgb_index);
            break;
        }
    }
}
//根据像素位置判断是否为内部点，以及点的类型（左边界、右边界、上边界、下边界、内部点，左上角，右上，左下，右下）
SeamlessClone::point_Type SeamlessClone::point_type(int x,int y){ //类中定义变量名称
    if(x == 0 && y == 0){
        return LU;
    }else if(x == 0 && y > 0 && y < H - 1){
        return L;
    }else if(x == 0 && y == H - 1){
        return LD;
    }else if(x > 0 && x < W - 1 && y == 0){
        return U;
    }else if(x == W - 1 && y == 0){
        return RU;
    }else if(x > 0 && x < W - 1 && y > 0 && y < H - 1){
        return IN;
    }else if(x == W - 1 && y > 0 && y < H - 1){
        return R;
    }else if(x > 0 && x < W - 1 && y == H - 1){
        return D;
    }else if(x == W - 1 && y == H - 1){
        return RD;
    }
}

// g(x,y)函数：从源图像src_img_中取值
// 输入(x,y)是目标图像中的坐标，需要转换为源图像坐标：(x - offset_x_, y - offset_y_)
// 返回源图像在该位置的RGB三元组
std::tuple<double, double, double> SeamlessClone::g(int x, int y){
    // 内部处理，把(x,y)对应源图像的坐标：
    int src_x = x + origin_x_;
    int src_y = y + origin_y_;
    
    // 边界检查
    if(src_x < 0 || src_x >= src_img_->width() || src_y < 0 || src_y >= src_img_->height()){
        return std::make_tuple(0.0, 0.0, 0.0);
    }
    
    // 获取像素索引
    int idx = (src_y * src_img_->width() + src_x) * 4;
    
    // 取RGB三通道的值
    double r = static_cast<double>(src_img_->data()[idx]);
    double g_val = static_cast<double>(src_img_->data()[idx + 1]);
    double b = static_cast<double>(src_img_->data()[idx + 2]);
    
    //返回三元组：
    return std::make_tuple(r,g_val,b);
}

// f(x,y)函数：从目标图像tar_img_中取值
// 输入(x,y)是目标图像中的坐标，返回该位置指定通道的像素值
// rgb_index: 0=R, 1=G, 2=B
double SeamlessClone::f(int x, int y, int rgb_index){
    //基于目标图像 + 选中区域偏移
    x = x + offset_x_; 
    y = y + offset_y_;
    // 边界检查
    if(x < 0 || x >= tar_img_->width() || y < 0 || y >= tar_img_->height()){
        return 0.0;
    }
    
    // 获取像素索引
    int idx = (y * tar_img_->width() + x) * 4;
    
    // 根据rgb_index返回对应通道的值
    if(rgb_index == 0){
        return static_cast<double>(tar_img_->data()[idx]);  // R
    }else if(rgb_index == 1){
        return static_cast<double>(tar_img_->data()[idx + 1]);  // G
    }else if(rgb_index == 2){
        return static_cast<double>(tar_img_->data()[idx + 2]);  // B
    }
    
    return 0.0;
}

std::shared_ptr<Image> SeamlessClone::solve() {
    // 为每个RGB通道求解方程组
    Eigen::VectorXd solutions[3];
    
    for(int rgb_index = 0; rgb_index < 3; rgb_index++){
        // 清空三元组列表
        triplet_list.clear();
        
        // 构建稀疏矩阵A和向量B
        for(int y = 0; y < H; ++y){
            for(int x = 0; x < W; ++x){
                fill_coefficient(x, y, rgb_index);
            }
        }
        
        // 三元组构建稀疏方程
        A.setFromTriplets(triplet_list.begin(), triplet_list.end());
        
        // 使用SimplicialLDLT求解器求解线性方程组
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
        solver.compute(A);
        
        if(solver.info() != Eigen::Success){
            std::cerr << "Solver failed!" << std::endl;
            return tar_img_;
        }
        
        solutions[rgb_index] = solver.solve(B);
    }
    
    // 将解应用到目标图像的选中区域
    for(int y = 0; y < H; ++y){
        for(int x = 0; x < W; ++x){
            // 计算在目标图像中的实际位置
            int tar_x = x + offset_x_;
            int tar_y = y + offset_y_;
            
            // 边界检查
            if(tar_x < 0 || tar_x >= tar_img_->width() || tar_y < 0 || tar_y >= tar_img_->height()){
                continue;
            }
            
            //获取mask值，只处理选中区域
            int mask_idx = (y * W + x) * src_selected_mask_->channels();
            if(src_selected_mask_->data()[mask_idx] == 0){
                continue;
            }
            
            // 获取三个通道的解
            int idx = y * W + x;
            double r_val = solutions[0][idx];
            double g_val = solutions[1][idx];
            double b_val = solutions[2][idx];
            
            // 裁剪到[0, 255]范围
            unsigned char r = static_cast<unsigned char>(std::clamp(r_val, 0.0, 255.0));
            unsigned char g = static_cast<unsigned char>(std::clamp(g_val, 0.0, 255.0));
            unsigned char b = static_cast<unsigned char>(std::clamp(b_val, 0.0, 255.0));
            
            // 设置像素值
            tar_img_->set_pixel(tar_x, tar_y, {r, g, b});
        }
    }
    
    return tar_img_;
}


}