#pragma once

#include "warper.h"
#include <vector>
#include <dlib/dnn.h>
#include <dlib/data_io.h>
#include <iostream>

using namespace std;
using namespace dlib;

namespace USTC_CG
{

// 定义神经网络结构
// 输入维度: 2 (x, y 坐标)
// 输出维度: 2 (变形后的 x', y' 坐标)
// 隐藏层: 两个 10 维的全连接层，使用 ReLU 激活函数
using warping_net = loss_mean_squared_multioutput<
    fc<2,                  // 输出层: 2 维 (x', y')
    relu<fc<10,            // 隐藏层 2: 10 维 + ReLU 激活
    relu<fc<10,            // 隐藏层 1: 10 维 + ReLU 激活
    input<matrix<float>>   // 输入层: 2 维 (x, y)
    >>>>>>;

class NNWarper : public Warper
{
public:
    NNWarper();
    virtual ~NNWarper() = default;

    void warp(const unsigned char* src, unsigned char* dst, int width, int height);

    void set_control_points(const std::vector<pair<float, float>>& start_points, 
                           const std::vector<pair<float, float>>& end_points);

    // 训练神经网络
    void train_network(int epochs = 1000, double learning_rate = 0.001);

    // 设置神经网络参数
    void set_network_params(int hidden1_size = 10, int hidden2_size = 10);

    // 保存/加载网络
    void save_network(const std::string& filename);
    void load_network(const std::string& filename);

    // 获取训练信息
    bool is_trained() const { return is_trained_; }
    int get_training_epochs() const { return training_epochs_; }

private:
    std::vector<pair<float, float>> start_points_;
    std::vector<pair<float, float>> end_points_;

    warping_net net_;
    bool is_trained_;
    int training_epochs_;

    // 准备训练数据
    void prepare_training_data(std::vector<matrix<float>>& inputs, 
                              std::vector<matrix<float>>& targets);

    // 计算变形后的坐标
    pair<float, float> function(float x, float y);
};

}  // namespace USTC_CG