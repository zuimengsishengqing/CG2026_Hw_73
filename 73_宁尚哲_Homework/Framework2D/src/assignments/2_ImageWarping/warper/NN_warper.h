#pragma once

#ifndef _HAS_STD_BYTE
#define _HAS_STD_BYTE 0
#endif

#include "warper.h"
#include <vector>
#include <dlib/dnn.h>
#include <dlib/data_io.h>
#include <iostream>
#include <string>  // 补充引入 string 头文件
#include <utility> // 补充引入 pair 头文件

// 【删除这里原本的 using namespace std;】

// 不使用 using namespace dlib，避免与标准库的 byte 等类型冲突
// 使用 dlib:: 前缀显式指定 dlib 的类型和函数

// 定义 dlib::matrix 的完整类型别名
using dlib_matrix = dlib::matrix<float, 0L, 0L, dlib::default_memory_manager, dlib::row_major_layout>;

namespace USTC_CG
{

// 定义神经网络结构
using warping_net = dlib::loss_mean_squared_multioutput<    
    dlib::fc<2,
    dlib::relu<dlib::fc<10,
    dlib::relu<dlib::fc<10,
    dlib::input<dlib_matrix>                                
    >>>>>>;

class NNWarper : public Warper
{
public:
    NNWarper();
    virtual ~NNWarper() = default;

    void warp(const unsigned char* src, unsigned char* dst, int width, int height);

    // 补充 std:: 前缀
    void set_control_points(const std::vector<std::pair<float, float>>& start_points, 
                           const std::vector<std::pair<float, float>>& end_points);

    // 训练神经网络
    void train_network(int epochs = 1000, double learning_rate = 0.001);

    // 设置神经网络参数
    void set_network_params(int hidden1_size = 10, int hidden2_size = 10);

    // 保存/加载网络 (补充 std:: 前缀)
    void save_network(const std::string& filename);
    void load_network(const std::string& filename);

    // 获取训练信息
    bool is_trained() const { return is_trained_; }
    int get_training_epochs() const { return training_epochs_; }

private:
    // 补充 std:: 前缀
    std::vector<std::pair<float, float>> start_points_;
    std::vector<std::pair<float, float>> end_points_;

    warping_net net_;  
    bool is_trained_;
    int training_epochs_;

    // 准备训练数据
    void prepare_training_data(std::vector<dlib_matrix>& inputs, 
                              std::vector<dlib_matrix>& targets);

    // 计算变形后的坐标 (补充 std:: 前缀)
    std::pair<float, float> function(float x, float y);
};

}  // namespace USTC_CG