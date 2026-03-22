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
    
    // 初始化预分解相关变量
    is_precomputed_ = false;
    num_pixels_ = 0;
    
    // 初始化coord_to_idx_
    coord_to_idx_ = std::vector<std::vector<int>>(H, std::vector<int>(W, -1));
    
    // 初始化混合梯度标志
    is_mixed_gradient_ = false;
}
// 填写 (x, y) 对应的方程系数
void SeamlessClone::fill_coefficient(int x,int y,int rgb_index, const std::vector<std::vector<int>>& coord_to_idx, bool fill_triplet, bool is_mixed_gradient){
    int idx = coord_to_idx[y][x];
    
    // 动态邻居检测：检查四个方向是否在选中区域内
    bool has_up = (y - 1 >= 0 && coord_to_idx[y-1][x] >= 0);
    bool has_down = (y + 1 < H && coord_to_idx[y+1][x] >= 0);
    bool has_left = (x - 1 >= 0 && coord_to_idx[y][x-1] >= 0);
    bool has_right = (x + 1 < W && coord_to_idx[y][x+1] >= 0);
    
    // 注意：不再将内部边缘点强行截断为 Dirichlet 边界。
    // 所有传入此函数的点（均在 mask 内）都使用 Poisson 方程。
    // 对于落在 mask 外的邻居，将其作为已知边界，取目标图像对应的像素值移到等式右侧 B 向量中。
    
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
    
    double gradient_rhs = 0.0;
    
    if(is_mixed_gradient){
        // 混合梯度模式：比较源图像和目标图像的梯度，选择较大的
        // 计算源图像的梯度
        double g_grad_up = g_center - g_up;
        double g_grad_down = g_center - g_down;
        double g_grad_left = g_center - g_left;
        double g_grad_right = g_center - g_right;
        
        // 计算目标图像的梯度（使用当前偏移量）
        double f_center = f(x, y, rgb_index);
        double f_up = f(x, y - 1, rgb_index);
        double f_down = f(x, y + 1, rgb_index);
        double f_left = f(x - 1, y, rgb_index);
        double f_right = f(x + 1, y, rgb_index);
        
        double f_grad_up = f_center - f_up;
        double f_grad_down = f_center - f_down;
        double f_grad_left = f_center - f_left;
        double f_grad_right = f_center - f_right;
        
        // 比较梯度绝对值，选择较大的
        double v_up = (std::abs(g_grad_up) >= std::abs(f_grad_up)) ? g_grad_up : f_grad_up;
        double v_down = (std::abs(g_grad_down) >= std::abs(f_grad_down)) ? g_grad_down : f_grad_down;
        double v_left = (std::abs(g_grad_left) >= std::abs(f_grad_left)) ? g_grad_left : f_grad_left;
        double v_right = (std::abs(g_grad_right) >= std::abs(f_grad_right)) ? g_grad_right : f_grad_right;
        
        // 使用混合梯度计算引导场散度
        gradient_rhs = v_up + v_down + v_left + v_right;
    }else{
        // 普通无缝融合模式：只使用源图像的梯度
        gradient_rhs = 4.0 * g_center - g_up - g_down - g_left - g_right;
    }
    
    // 设置中心点对角线元素（未知数）系数始终为 4
    if(fill_triplet){
        triplet_list.push_back(Eigen::Triplet<double>(idx, idx, 4.0));
    }
    
    // 处理四个方向的邻居判定：
    // 如果邻居在 mask 内，向稀疏矩阵填入 -1.0
    // 如果邻居在 mask 外，去目标图像取色并累加到右侧向量
    
    // 上
    if(has_up){
        if(fill_triplet) triplet_list.push_back(Eigen::Triplet<double>(idx, coord_to_idx[y-1][x], -1.0));
    } else {
        gradient_rhs += f(x, y - 1, rgb_index);
    }
    
    // 下
    if(has_down){
        if(fill_triplet) triplet_list.push_back(Eigen::Triplet<double>(idx, coord_to_idx[y+1][x], -1.0));
    } else {
        gradient_rhs += f(x, y + 1, rgb_index);
    }
    
    // 左
    if(has_left){
        if(fill_triplet) triplet_list.push_back(Eigen::Triplet<double>(idx, coord_to_idx[y][x-1], -1.0));
    } else {
        gradient_rhs += f(x - 1, y, rgb_index);
    }
    
    // 右
    if(has_right){
        if(fill_triplet) triplet_list.push_back(Eigen::Triplet<double>(idx, coord_to_idx[y][x+1], -1.0));
    } else {
        gradient_rhs += f(x + 1, y, rgb_index);
    }
    
    // 设置右侧向量 B
    B(idx) = gradient_rhs;
}

void SeamlessClone::set_mixed_gradient(bool flag){
    is_mixed_gradient_ = flag;
}
//根据像素位置判断是否为内部点，以及点的类型（左边界、右边界、上边界、下边界、内部点，左上角，右上，左下，右下）
SeamlessClone::point_Type SeamlessClone::point_type(int x,int y){ //类中定义变量名称
    // 边界检查：确保坐标在有效范围内
    if(x < 0 || x >= W || y < 0 || y >= H){
        return IN; // 超出范围，默认返回内部点（在fill_coefficient中会进一步检查）
    }
    
    // 四个角点
    if(x == 0 && y == 0){
        return LU;
    }else if(x == W - 1 && y == 0){
        return RU;
    }else if(x == 0 && y == H - 1){
        return LD;
    }else if(x == W - 1 && y == H - 1){
        return RD;
    }
    // 四条边（非角点）
    else if(x == 0){
        return L;
    }else if(x == W - 1){
        return R;
    }else if(y == 0){
        return U;
    }else if(y == H - 1){
        return D;
    }
    // 内部点
    else{
        return IN;
    }
}

// g(x,y)函数：从源图像src_img_中取值
// 输入(x,y)是目标图像中的坐标，需要转换为源图像坐标：(x - offset_x_, y - offset_y_)
// 返回源图像在该位置的RGB三元组
std::tuple<double, double, double> SeamlessClone::g(int x, int y){
    // 内部处理，把(x,y)对应源图像的坐标：
    int src_x = x;
    int src_y = y;
    
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

void SeamlessClone::precompute() {
    // 第一步：统计选中区域内像素数量，建立坐标到矩阵索引的映射
    // 这是不规则区域处理的核心：
    // - selected_pixels_: 存储所有选中像素的坐标 (x, y)
    // - coord_to_idx_: 建立二维坐标到一维索引的映射，未选中区域标记为 -1
    // 这样可以将不规则区域映射为连续的索引，便于矩阵运算
    selected_pixels_.clear();
    coord_to_idx_ = std::vector<std::vector<int>>(H, std::vector<int>(W, -1));
    
    for(int y = 0; y < H; ++y){
        for(int x = 0; x < W; ++x){
            int mask_idx = (y * W + x) * src_selected_mask_->channels();
            if(src_selected_mask_->data()[mask_idx] > 0){
                coord_to_idx_[y][x] = selected_pixels_.size();
                selected_pixels_.push_back({x, y});
            }
        }
    }
    
    num_pixels_ = selected_pixels_.size();
    if(num_pixels_ == 0){
        is_precomputed_ = false;
        return;
    }
    
    // 重新设置矩阵A的维度
    A = Eigen::SparseMatrix<double>(num_pixels_, num_pixels_);
    
    // 只为第一个RGB通道构建系数矩阵A（三个通道的A矩阵相同）
    // 注意：对于不规则区域，矩阵A的结构由区域的形状决定
    // - 内部点：对角线为4，邻居为-1
    // - 边界点：对角线为1，其他为0（Dirichlet边界条件）
    triplet_list.clear();
    for(const auto& pixel : selected_pixels_){
        int x = pixel.first;
        int y = pixel.second;
        fill_coefficient(x, y, 0, coord_to_idx_, true, is_mixed_gradient_);
    }
    
    // 三元组构建稀疏方程
    A.setFromTriplets(triplet_list.begin(), triplet_list.end());
    
    // 预分解矩阵A
    // 对于不规则区域，矩阵A的结构一旦确定就不会改变（除非区域形状改变）
    // 因此可以预分解，在实时编辑时只需更新右侧向量B
    solver_.compute(A);
    
    if(solver_.info() != Eigen::Success){
        std::cerr << "Precomputation failed! Matrix may be singular." << std::endl;
        is_precomputed_ = false;
    }else{
        is_precomputed_ = true;
    }
}

void SeamlessClone::update_offset(int new_offset_x, int new_offset_y) {
    offset_x_ = new_offset_x;
    offset_y_ = new_offset_y;
}

std::shared_ptr<Image> SeamlessClone::solve_fast() {
    if(!is_precomputed_ || num_pixels_ == 0){
        return tar_img_;
    }
    
    // 为每个RGB通道求解方程组
    // 注意：对于不规则区域，矩阵A已经预分解，只需要更新右侧向量B
    // 右侧向量B依赖于目标图像的像素值，会随offset改变而改变
    Eigen::VectorXd solutions[3];
    
    for(int rgb_index = 0; rgb_index < 3; rgb_index++){
        // 重新设置向量B的维度
        B = Eigen::VectorXd(num_pixels_);
        
        // 只构建右侧向量B（矩阵A已经预分解）
        // 对于不规则区域：
        // - 内部点：B = sum(v_pq)，其中 v_pq 是引导梯度（源图像梯度或混合梯度）
        // - 边界点：B = f*_p（目标图像在该位置的像素值）
        for(const auto& pixel : selected_pixels_){
            int x = pixel.first;
            int y = pixel.second;
            
            // 重新计算B向量（因为offset可能改变），混合梯度模式下必须使用最新offset
            fill_coefficient(x, y, rgb_index, coord_to_idx_, false, is_mixed_gradient_);
        }
        
        // 使用预分解的求解器快速求解
        // 由于矩阵A已经预分解，这里只需要执行前代和后代，速度非常快
        solutions[rgb_index] = solver_.solve(B);
        
        if(solver_.info() != Eigen::Success){
            std::cerr << "Fast solve failed!" << std::endl;
            return tar_img_;
        }
    }
    
    // 将解应用到目标图像的选中区域
    // 对于不规则区域，只更新掩码内的像素
    for(const auto& pixel : selected_pixels_){
        int x = pixel.first;
        int y = pixel.second;
        
        // 计算在目标图像中的实际位置
        int tar_x = x + offset_x_;
        int tar_y = y + offset_y_;
        
        // 边界检查
        if(tar_x < 0 || tar_x >= tar_img_->width() || tar_y < 0 || tar_y >= tar_img_->height()){
            continue;
        }
        
        // 获取三个通道的解
        int idx = coord_to_idx_[y][x];
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
    
    return tar_img_;
}

std::shared_ptr<Image> SeamlessClone::solve() {
    // 第一步：统计选中区域内像素数量，建立坐标到矩阵索引的映射
    // 这是不规则区域处理的核心：
    // - selected_pixels: 存储所有选中像素的坐标 (x, y)
    // - coord_to_idx: 建立二维坐标到一维索引的映射，未选中区域标记为 -1
    std::vector<std::pair<int, int>> selected_pixels;
    std::vector<std::vector<int>> coord_to_idx(H, std::vector<int>(W, -1));
    
    for(int y = 0; y < H; ++y){
        for(int x = 0; x < W; ++x){
            int mask_idx = (y * W + x) * src_selected_mask_->channels();
            if(src_selected_mask_->data()[mask_idx] > 0){
                coord_to_idx[y][x] = selected_pixels.size();
                selected_pixels.push_back({x, y});
            }
        }
    }
    
    int num_pixels = selected_pixels.size();
    if(num_pixels == 0){
        return tar_img_;
    }
    
    // 为每个RGB通道求解方程组
    Eigen::VectorXd solutions[3];
    
    for(int rgb_index = 0; rgb_index < 3; rgb_index++){
        // 清空三元组列表
        triplet_list.clear();
        
        // 重新设置矩阵A和向量B的维度
        A = Eigen::SparseMatrix<double>(num_pixels, num_pixels);
        B = Eigen::VectorXd(num_pixels);
        
        // 构建稀疏矩阵A和向量B
        // 对于不规则区域：
        // - 内部点：A的对角线为4，邻居为-1；B = sum(v_pq)
        // - 边界点：A的对角线为1，其他为0；B = f*_p
        for(const auto& pixel : selected_pixels){
            int x = pixel.first;
            int y = pixel.second;
            fill_coefficient(x, y, rgb_index, coord_to_idx, true, is_mixed_gradient_);
        }
        
        // 三元组构建稀疏方程
        A.setFromTriplets(triplet_list.begin(), triplet_list.end());
        
        // 使用SimplicialLDLT求解器求解线性方程组
        // SimplicialLDLT 适用于对称正定矩阵，Poisson方程的系数矩阵满足这个条件
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
        solver.compute(A);
        
        if(solver.info() != Eigen::Success){
            std::cerr << "Solver failed! Matrix may be singular." << std::endl;
            return tar_img_;
        }
        
        solutions[rgb_index] = solver.solve(B);
    }
    
    // 将解应用到目标图像的选中区域
    // 对于不规则区域，只更新掩码内的像素
    for(const auto& pixel : selected_pixels){
        int x = pixel.first;
        int y = pixel.second;
        
        // 计算在目标图像中的实际位置
        int tar_x = x + offset_x_;
        int tar_y = y + offset_y_;
        
        // 边界检查
        if(tar_x < 0 || tar_x >= tar_img_->width() || tar_y < 0 || tar_y >= tar_img_->height()){
            continue;
        }
        
        // 获取三个通道的解
        int idx = coord_to_idx[y][x];
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
    
    return tar_img_;
}


}