// utils/min_surf_floater.cpp
#include "min_surf_floater.h"
#include <Eigen/SparseLU>
#include <OpenMesh/Core/Geometry/VectorT.hh>
#include <cmath>
#define M_PI 3.14159265358979323846

USTC_CG_NAMESPACE_OPEN_SCOPE

MinSurfFloater::MinSurfFloater(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original) 
    : TutteParameterizer(mesh, original) {}

void MinSurfFloater::compute_local_params(int vi, std::vector<int>& indices, 
    std::vector<float>& angles, std::vector<float>& distances) {
    indices.clear();
    angles.clear();
    distances.clear();

    auto vh = mesh_->vertex_handle(vi);
    auto pi = original_mesh_->point(vh);

    // 收集邻居信息
    for (auto voh_it = mesh_->voh_iter(vh); voh_it.is_valid(); ++voh_it) {
        int j = mesh_->to_vertex_handle(*voh_it).idx();
        indices.push_back(j);
        auto pj = original_mesh_->point(mesh_->vertex_handle(j));
        float dist = (pj - pi).norm();
        distances.push_back(dist);
    }

    // 计算角度
    int degree = indices.size();
    float total_angle = 0.0f;
    angles.resize(degree);
    for (int k = 0; k < degree; ++k) {
        int k_prev = (k - 1 + degree) % degree;
        int k_curr = k;
        auto p_prev = original_mesh_->point(mesh_->vertex_handle(indices[k_prev]));
        auto p_curr = original_mesh_->point(mesh_->vertex_handle(indices[k_curr]));
        auto v1 = p_prev - pi;
        auto v2 = p_curr - pi;
        float cos_alpha = (v1 | v2) / (v1.norm() * v2.norm());
        cos_alpha = std::max(-1.0f, std::min(1.0f, cos_alpha));
        float angle = std::acos(cos_alpha);
        angles[k] = angle;
        total_angle += angle;
    }

    // 归一化角度到 2π
    for (int k = 0; k < degree; ++k) {
        angles[k] = 2.0f * M_PI * angles[k] / total_angle;
    }
}

double MinSurfFloater::compute_shape_preserving_weight(int vi, int vj) {
    std::vector<int> indices;
    std::vector<float> angles;
    std::vector<float> distances;
    compute_local_params(vi, indices, angles, distances);

    int degree = indices.size();
    if (degree < 2) return 0.0; // 无效情况，返回 0

    // 处理度数为 2 的特殊情况
    if (degree == 2) {
        auto it = std::find(indices.begin(), indices.end(), vj);
        return it != indices.end() ? 0.5 : 0.0; // 均匀分配权重
    }

    // 生成局部二维坐标
    std::vector<OpenMesh::Vec2f> p_neighbors(degree);
    float current_angle = 0.0f;
    for (int k = 0; k < degree; ++k) {
        current_angle += angles[k];
        float r = distances[k];
        p_neighbors[k] = OpenMesh::Vec2f(r * std::cos(current_angle), r * std::sin(current_angle));
    }
    OpenMesh::Vec2f p(0.0f, 0.0f); // 中心点

    // 定义叉积函数
    auto cross2d = [](OpenMesh::Vec2f a, OpenMesh::Vec2f b) {
        return a[0] * b[1] - a[1] * b[0];
    };

    // 计算形状保持权重
    std::vector<float> lambda(degree, 0.0f);
    float weight_sum = 0.0f;
    for (int i = 0; i < degree; ++i) {
        int back_ind = (i + 1) % degree;
        // 使用叉积判断射线交点
        while (back_ind % degree == i ||
               cross2d(p_neighbors[i], p_neighbors[i] - p_neighbors[back_ind % degree]) *
                   cross2d(p_neighbors[i], p_neighbors[i] - p_neighbors[(back_ind + 1) % degree]) > 0) {
            back_ind++;
        }
        auto p1 = p_neighbors[i];
        auto p2 = p_neighbors[back_ind % degree];
        auto p3 = p_neighbors[(back_ind + 1) % degree];
        float s1 = std::abs(cross2d(p2, p3));
        float s2 = std::abs(cross2d(p3, p1));
        float s3 = std::abs(cross2d(p1, p2));
        float s = s1 + s2 + s3;
        if (s < 1e-6f) continue; // 跳过退化三角形
        lambda[i] += s1 / s;
        lambda[back_ind % degree] += s2 / s;
        lambda[(back_ind + 1) % degree] += s3 / s;
        weight_sum += 1.0f;
    }

    // 找到 vj 的索引并返回权重
    auto it = std::find(indices.begin(), indices.end(), vj);
    if (it == indices.end()) return 0.0;
    int vj_idx = std::distance(indices.begin(), it);

    // 如果 weight_sum 有效，返回平均权重；否则退为均匀权重
    return weight_sum > 0 ? lambda[vj_idx] / weight_sum : 1.0 / degree;
}

void MinSurfFloater::execute() {
    int n_vertices = mesh_->n_vertices();
    std::vector<Eigen::Triplet<double>> triplets;
    Eigen::VectorXd bx(n_vertices), by(n_vertices), bz(n_vertices);
    bx.setZero();
    by.setZero();
    bz.setZero();

    for (auto vh : mesh_->vertices()) {
        int i = vh.idx();
        auto pos = mesh_->point(vh);
        if (is_boundary_[i]) {
            triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));
            bx[i] = pos[0];
            by[i] = pos[1];
            bz[i] = pos[2];
        } else {
            double weight_sum = 0.0;
            std::vector<std::pair<int, double>> neighbors;
            for (auto voh_it = mesh_->voh_iter(vh); voh_it.is_valid(); ++voh_it) {
                int j = mesh_->to_vertex_handle(*voh_it).idx();
                double weight = compute_shape_preserving_weight(i, j);
                if (weight < 0 || std::isnan(weight) || std::isinf(weight)) weight = 1e-6; // 最小正值保护
                neighbors.push_back({j, weight});
                weight_sum += weight;
            }
            if (weight_sum < 1e-6) weight_sum = 1.0; // 防止除零
            for (const auto& [j, w] : neighbors) {
                triplets.push_back(Eigen::Triplet<double>(i, j, w)); // 邻居权重为正
            }
            triplets.push_back(Eigen::Triplet<double>(i, i, -weight_sum)); // 自身系数为负
        }
    }

    Eigen::SparseMatrix<double> L(n_vertices, n_vertices);
    L.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success) throw std::runtime_error("SparseLU decomposition failed!");

    Eigen::VectorXd x = solver.solve(bx);
    Eigen::VectorXd y = solver.solve(by);
    Eigen::VectorXd z = solver.solve(bz);

    update_mesh(x, y, z);
}

USTC_CG_NAMESPACE_CLOSE_SCOPE