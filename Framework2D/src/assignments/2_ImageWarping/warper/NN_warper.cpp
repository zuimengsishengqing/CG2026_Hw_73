#include "NN_warper.h"
#include <algorithm>
#include <cmath>

using namespace std;
using namespace dlib;

namespace USTC_CG
{

NNWarper::NNWarper() : is_trained_(false), training_epochs_(0)
{
    // 网络会在第一次训练时初始化
}

void NNWarper::set_control_points(const std::vector<pair<float, float>>& start_points,
                                  const std::vector<pair<float, float>>& end_points)
{
    start_points_ = start_points;
    end_points_ = end_points;
    is_trained_ = false;  // 控制点改变后需要重新训练
}

void NNWarper::prepare_training_data(std::vector<matrix<float>>& inputs,
                                     std::vector<matrix<float>>& targets)
{
    inputs.clear();
    targets.clear();

    // 使用控制点作为训练数据
    for (size_t i = 0; i < start_points_.size(); ++i) {
        // 输入: 原始坐标 (x, y)
        matrix<float> input(2, 1);
        input(0, 0) = start_points_[i].first;
        input(1, 0) = start_points_[i].second;
        inputs.push_back(input);

        // 目标: 变形后的坐标 (x', y')
        matrix<float> target(2, 1);
        target(0, 0) = end_points_[i].first;
        target(1, 0) = end_points_[i].second;
        targets.push_back(target);
    }

    // 如果控制点较少，可以添加一些插值点来增强训练
    // 这里我们使用简单的线性插值在控制点之间生成额外训练点
    if (start_points_.size() >= 2) {
        for (size_t i = 0; i < start_points_.size(); ++i) {
            for (size_t j = i + 1; j < start_points_.size(); ++j) {
                // 在每对控制点之间生成插值点
                for (float t = 0.25f; t < 1.0f; t += 0.25f) {
                    // 插值输入
                    matrix<float> input(2, 1);
                    input(0, 0) = start_points_[i].first * (1.0f - t) + start_points_[j].first * t;
                    input(1, 0) = start_points_[i].second * (1.0f - t) + start_points_[j].second * t;
                    inputs.push_back(input);

                    // 插值目标
                    matrix<float> target(2, 1);
                    target(0, 0) = end_points_[i].first * (1.0f - t) + end_points_[j].first * t;
                    target(1, 0) = end_points_[i].second * (1.0f - t) + end_points_[j].second * t;
                    targets.push_back(target);
                }
            }
        }
    }

    cout << "Prepared " << inputs.size() << " training samples" << endl;
}

void NNWarper::train_network(int epochs, double learning_rate)
{
    if (start_points_.empty() || end_points_.empty()) {
        cerr << "Error: No control points set for training" << endl;
        return;
    }

    if (start_points_.size() != end_points_.size()) {
        cerr << "Error: Start and end points count mismatch" << endl;
        return;
    }

    cout << "Training neural network with " << start_points_.size() << " control points..." << endl;

    // 准备训练数据
    std::vector<matrix<float>> inputs;
    std::vector<matrix<float>> targets;
    prepare_training_data(inputs, targets);

    // 配置优化器
    dnn_trainer<warping_net, adam> trainer(net_);
    trainer.set_learning_rate(learning_rate);
    trainer.set_min_learning_rate(1e-5);
    trainer.set_mini_batch_size(std::min(32, (int)inputs.size()));
    trainer.be_verbose();

    // 训练网络
    cout << "Starting training for " << epochs << " epochs..." << endl;
    trainer.train(inputs, targets);

    is_trained_ = true;
    training_epochs_ = epochs;

    cout << "Training completed!" << endl;
    cout << "Final loss: " << trainer.get_average_loss() << endl;
}

void NNWarper::set_network_params(int hidden1_size, int hidden2_size)
{
    // 重新定义网络结构需要重新编译，这里只是一个占位符
    // 实际使用时需要在头文件中重新定义网络类型
    cout << "Network parameters: hidden1=" << hidden1_size << ", hidden2=" << hidden2_size << endl;
    is_trained_ = false;
}

pair<float, float> NNWarper::function(float x, float y)
{
    if (!is_trained_) {
        cerr << "Warning: Network not trained, returning identity mapping" << endl;
        return {x, y};
    }

    // 准备输入
    matrix<float> input(2, 1);
    input(0, 0) = x;
    input(1, 0) = y;

    // 前向传播
    matrix<float> output = net_(input);

    return {output(0, 0), output(1, 0)};
}

void NNWarper::warp(const unsigned char* src, unsigned char* dst, int width, int height)
{
    // 如果网络未训练，先训练
    if (!is_trained_ && !start_points_.empty()) {
        cout << "Network not trained, training now..." << endl;
        train_network();
    }

    if (!is_trained_) {
        cerr << "Error: Cannot warp - network not trained and no control points" << endl;
        return;
    }

    int out_of_bounds_count = 0;
    int total_pixels = 0;

    // 对每个像素应用变形
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            total_pixels++;

            // 计算变形后的坐标
            auto [new_x, new_y] = function(x, y);

            // 检查坐标是否在图像范围内
            if (new_x >= 0 && new_x < width && new_y >= 0 && new_y < height) {
                // 获取原图像像素
                int src_index = (y * width + x) * 4;  // RGBA格式
                // 设置目标图像像素
                int dst_index = (static_cast<int>(new_y) * width + static_cast<int>(new_x)) * 4;
                dst[dst_index] = src[src_index];
                dst[dst_index + 1] = src[src_index + 1];
                dst[dst_index + 2] = src[src_index + 2];
                dst[dst_index + 3] = src[src_index + 3];
            } else {
                out_of_bounds_count++;
            }
        }
    }

    cout << "Neural Network Warping completed" << endl;
    cout << "Total pixels: " << total_pixels << ", Out of bounds: " << out_of_bounds_count << endl;
}

void NNWarper::save_network(const std::string& filename)
{
    if (!is_trained_) {
        cerr << "Error: Cannot save untrained network" << endl;
        return;
    }

    try {
        serialize(filename) << net_;
        cout << "Network saved to: " << filename << endl;
    } catch (const std::exception& e) {
        cerr << "Error saving network: " << e.what() << endl;
    }
}

void NNWarper::load_network(const std::string& filename)
{
    try {
        deserialize(filename) >> net_;
        is_trained_ = true;
        cout << "Network loaded from: " << filename << endl;
    } catch (const std::exception& e) {
        cerr << "Error loading network: " << e.what() << endl;
        is_trained_ = false;
    }
}

}  // namespace USTC_CG