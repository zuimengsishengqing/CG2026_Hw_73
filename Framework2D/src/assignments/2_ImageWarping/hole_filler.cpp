#include "hole_filler.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <queue>
#include <limits>
// 修复1：替换为正确的头文件（移除random.h，包含kissrandom.h）
#include "annoylib.h"
#include "kissrandom.h"

namespace USTC_CG
{

HoleFiller::HoleFiller() : search_radius_(10), method_(0)
{
}

HoleFiller::~HoleFiller()
{
}

bool HoleFiller::is_hole_pixel(const unsigned char* image, int width, int height, int x, int y)
{
    int index = (y * width + x) * 4;
    return (image[index] == 0 && image[index + 1] == 0 && image[index + 2] == 0);
}

std::vector<std::pair<int, int>> HoleFiller::find_known_pixels(
    const unsigned char* image, int width, int height, 
    int x, int y, int radius)
{
    std::vector<std::pair<int, int>> known_pixels;
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (!is_hole_pixel(image, width, height, nx, ny)) {
                    known_pixels.push_back({nx, ny});
                }
            }
        }
    }
    return known_pixels;
}

#ifdef USE_ANNOY
std::vector<std::pair<int, int>> HoleFiller::find_known_pixels_annoy(
    const unsigned char* image, int width, int height, 
    int x, int y, int radius)
{
    std::vector<std::pair<int, int>> all_known_pixels;
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            if (!is_hole_pixel(image, width, height, px, py)) {
                all_known_pixels.push_back({px, py});
            }
        }
    }

    if (all_known_pixels.empty()) return {};

    // 修复2：修正AnnoyIndex模板参数
    // 模板参数说明（匹配Annoy官方定义）：
    // 1. int: 索引类型（用于标识每个数据点）
    // 2. float: 向量元素类型（x/y坐标是float类型）
    // 3. Annoy::Euclidean: 距离计算类型（欧式距离）
    // 4. Annoy::Kiss32Random: 随机数生成器
    // 5. Annoy::AnnoyIndexSingleThreadedBuildPolicy: 线程构建策略
    Annoy::AnnoyIndex<int, float, Annoy::Euclidean, Annoy::Kiss32Random, Annoy::AnnoyIndexSingleThreadedBuildPolicy> index(2); // 2表示向量维度（x,y）
    
    for (size_t i = 0; i < all_known_pixels.size(); ++i) {
        float vec[2] = {static_cast<float>(all_known_pixels[i].first), 
                        static_cast<float>(all_known_pixels[i].second)};
        // 修复3：移除冗余const_cast，float*可隐式转为const float*
        index.add_item(static_cast<int>(i), vec);
    }
    
    index.build(10); // 构建索引，10棵随机树

    float query_vec[2] = {static_cast<float>(x), static_cast<float>(y)};
    std::vector<int> nearest_indices;
    std::vector<float> distances;
    // 获取最近邻：50个邻居，-1无搜索限制
    index.get_nns_by_vector(query_vec, 50, -1, &nearest_indices, &distances);

    std::vector<std::pair<int, int>> known_pixels;
    for (int idx : nearest_indices) {
        if (calculate_distance(x, y, all_known_pixels[idx].first, all_known_pixels[idx].second) <= radius) {
            known_pixels.push_back(all_known_pixels[idx]);
        }
    }
    return known_pixels;
}
#else
std::vector<std::pair<int, int>> HoleFiller::find_known_pixels_annoy(
    const unsigned char* image, int width, int height, 
    int x, int y, int radius)
{
    return find_known_pixels(image, width, height, x, y, radius);
}
#endif

float HoleFiller::calculate_distance(int x1, int y1, int x2, int y2)
{
    float dx = static_cast<float>(x1 - x2);
    float dy = static_cast<float>(y1 - y2);
    return std::sqrt(dx * dx + dy * dy);
}

// 修复4：移除默认参数（头文件中已声明，避免重定义）
float HoleFiller::calculate_idw_weight(float distance, float power)
{
    if (distance < 1e-6f) return 1e10f;
    return 1.0f / std::pow(distance, power);
}

std::tuple<unsigned char, unsigned char, unsigned char> HoleFiller::interpolate_color(
    const unsigned char* image, int width, int height,
    const std::vector<std::pair<int, int>>& known_pixels,
    int target_x, int target_y)
{
    if (known_pixels.empty()) return {0, 0, 0};
    
    float sum_weights = 0.0f;
    float sum_r = 0.0f, sum_g = 0.0f, sum_b = 0.0f;
    
    for (const auto& pixel : known_pixels) {
        int px = pixel.first;
        int py = pixel.second;
        int idx = (py * width + px) * 4; // 避免和Annoy index重名，变量名改为idx
        
        float dist = calculate_distance(target_x, target_y, px, py);
        // 调用时显式传默认值（或头文件声明默认参数）
        float weight = calculate_idw_weight(dist, 2.0f);
        
        sum_weights += weight;
        sum_r += weight * image[idx];
        sum_g += weight * image[idx + 1];
        sum_b += weight * image[idx + 2];
    }
    
    if (sum_weights < 1e-6f) return {0, 0, 0};
    
    return {
        static_cast<unsigned char>(std::clamp(sum_r / sum_weights, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(sum_g / sum_weights, 0.0f, 255.0f)),
        static_cast<unsigned char>(std::clamp(sum_b / sum_weights, 0.0f, 255.0f))
    };
}

void HoleFiller::fill_holes(unsigned char* image, int width, int height, 
                           int search_radius, int method)
{
    search_radius_ = search_radius;
    method_ = method;
    
#ifdef USE_ANNOY
    std::cout << "Filling holes using Annoy..." << std::endl;
#else
    std::cout << "Filling holes using brute force..." << std::endl;
#endif

    std::vector<std::pair<int, int>> hole_positions;
    std::vector<std::pair<int, int>> all_known_pixels;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (is_hole_pixel(image, width, height, x, y)) 
                hole_positions.push_back({x, y});
            else
                all_known_pixels.push_back({x, y});
        }
    }

#ifdef USE_ANNOY
    // 修复5：统一AnnoyIndex模板参数（和上方保持一致）
    Annoy::AnnoyIndex<int, float, Annoy::Euclidean, Annoy::Kiss32Random, Annoy::AnnoyIndexSingleThreadedBuildPolicy> index(2);
    if (!all_known_pixels.empty()) {
        for (size_t i = 0; i < all_known_pixels.size(); ++i) {
            float vec[2] = {(float)all_known_pixels[i].first, (float)all_known_pixels[i].second};
            index.add_item(static_cast<int>(i), vec);
        }
        index.build(10);
    }
#endif

    for (const auto& hole : hole_positions) {
        int x = hole.first;
        int y = hole.second;
        std::vector<std::pair<int, int>> known_pixels;

#ifdef USE_ANNOY
        if (all_known_pixels.empty()) break; // 避免空索引访问
        float query_vec[2] = {(float)x, (float)y};
        std::vector<int> nearest_indices;
        // 修复6：简化get_nns_by_vector参数，distances传nullptr即可
        index.get_nns_by_vector(query_vec, 20, -1, &nearest_indices, nullptr);
        for (int idx : nearest_indices) {
            if (calculate_distance(x, y, all_known_pixels[idx].first, all_known_pixels[idx].second) <= search_radius_) {
                known_pixels.push_back(all_known_pixels[idx]);
            }
        }
#else
        known_pixels = find_known_pixels(image, width, height, x, y, search_radius_);
#endif

        if (!known_pixels.empty()) {
            auto [r, g, b] = interpolate_color(image, width, height, known_pixels, x, y);
            int idx = (y * width + x) * 4;
            image[idx] = r; 
            image[idx+1] = g; 
            image[idx+2] = b; 
            image[idx+3] = 255; // 不透明Alpha通道
        }
    }
}

} // namespace USTC_CG