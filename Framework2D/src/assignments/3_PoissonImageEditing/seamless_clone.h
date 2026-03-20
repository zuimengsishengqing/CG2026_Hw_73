#pragma once
#include "common/image.h"
#include <Eigen/Sparse>
#include <Eigen/Dense>

namespace USTC_CG{
class SeamlessClone
{
   public:
    // 构造函数
    SeamlessClone(std::shared_ptr<Image> src_img, std::shared_ptr<Image> tar_img, std::shared_ptr<Image> src_selected_mask, int offset_x, int offset_y, int origin_x, int origin_y);
    //其中src_img为源图像，tar_img为目标图像，src_selected_mask为选择区域，offset_x,offset_y为选择区域在目标图像中的位置（一般取左上角）
    std::shared_ptr<Image> solve(); // 给外部调用的接口，求解 Poisson 方程组，返回一个 Seamless Clone 的结果图像（和背景图像一样大，替换了选中区域）

    // 填写 (x, y) 对应的方程系数
    void fill_coefficient(int x,int y,int rgb_index);


   private:
    // 注意使用指针，避免额外的复制操作
    std::shared_ptr<Image> src_img_; // 源图像
    std::shared_ptr<Image> tar_img_; // 背景图像
    std::shared_ptr<Image> src_selected_mask_; // 选中区域（矩形情形可以无视）
    int offset_x_, offset_y_;        // 矩形区域在目标(target)图像中的位置（例如，左上角的坐标）
    int origin_x_, origin_y_;        // 矩形区域在源图像位置，左上角坐标；
    std::vector<Eigen::Triplet<double>> triplet_list; // 使用三元组 vector 来存储非零元素,(行,列,value)
    int W,H; //图像宽、高在类内存储
    Eigen::SparseMatrix<double> A; // 稀疏矩阵 A,系数矩阵
    Eigen::VectorXd B; // 向量 B,右侧向量
    //定义点类型对应字典：
    enum point_Type{
        IN = 0,//内部点
        LU = 1,//左上
        U = 2,//上边界
        RU = 3,//右上
        L = 4,//左边界
        R = 5,//右边界
        LD = 6,//左下
        D = 7,//下边界
        RD = 8,//右下
    };

    //根据像素位置判断是否为内部点，以及点的类型（左边界、右边界、上边界、下边界、内部点，左上角，右上，左下，右下）
    point_Type point_type(int x,int y);
    //g(x,y)函数取值。$g$ 表示源图像，实际取值的时候需要转换一下坐标
    //输入(x,y)是目标图像中的坐标，需要转换为源图像坐标：(x - offset_x_, y - offset_y_)
    //返回源图像在该位置的RGB三元组
    std::tuple<double, double, double> g(int x, int y);
    //f(x,y)函数取target image的图像值
    //输入(x,y)是目标图像中的坐标，返回该位置指定通道的像素值
    double f(int x, int y, int rgb_index);

};
}