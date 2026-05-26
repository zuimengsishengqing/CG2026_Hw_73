#include "ASAP.h"
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <Eigen/SVD>
#include <stdexcept>
#include <cmath>

USTC_CG_NAMESPACE_OPEN_SCOPE

ASAP::ASAP(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> param_mesh)
    : ARAP(mesh, param_mesh)
{
}

void ASAP::execute_asap() {
    // 若未预处理则先预处理（注意：ARAP::precompute 构造了 A、余切权重等矩阵）
    if (!precomputed_) {
        precompute();
    }
    
    // --- Local Phase ---
    // 对每个面计算局部相似变换（与 ARAP 不同，ARAP 只求旋转）
    tbb::parallel_for(tbb::blocked_range<size_t>(0, mesh_->n_faces()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                // 计算雅可比矩阵 J = u_coords * x_coords⁻¹
                Eigen::Matrix2f J = u_coords_[i] * x_coords_[i].inverse();
                // SVD 分解：J = U * Σ * Vᵀ
                Eigen::JacobiSVD<Eigen::Matrix2f> svd(J, Eigen::ComputeFullU | Eigen::ComputeFullV);
                float sigma1 = svd.singularValues()(0);
                float sigma2 = svd.singularValues()(1);
                // 取两个奇异值的平均作为全局缩放因子
                float s = (sigma1 + sigma2) * 0.5f;
                // 最优相似变换：L = U * (s * I) * Vᵀ
                transforms[i] = svd.matrixU() * (s * Eigen::Matrix2f::Identity()) * svd.matrixV().transpose();
            }
        });
    
    // --- Global Phase ---
    // 组装全局线性系统的右端项 b（与 ARAP 中相同，只是此处使用的是 similarity 变换）
    Eigen::MatrixXf b(mesh_->n_vertices(), 2);
    b.setZero();
    
    tbb::parallel_for(tbb::blocked_range<size_t>(0, mesh_->n_vertices()),
        [&](const tbb::blocked_range<size_t>& range) {
            for (size_t idx = range.begin(); idx != range.end(); ++idx) {
                auto vh = mesh_->vertex_handle(idx);
                int i = vh.idx();
                if (i == 0) continue;  // 固定顶点 0（作为约束）
                
                Eigen::Vector2f weight_sum = Eigen::Vector2f::Zero();
                // 获取该顶点的所有 outgoing halfedge
                auto heh_start = mesh_->halfedge_handle(vh);
                if (!heh_start.is_valid()) continue;
                
                auto heh = heh_start;
                do {
                    auto opp = mesh_->opposite_halfedge_handle(heh);
                    Eigen::Vector2f weight = Eigen::Vector2f::Zero();
                    
                    // 若当前半边不在边界，则加上对应贡献
                    if (!mesh_->is_boundary(heh))
                        weight += ctg_[heh.idx()] * transforms[mesh_->face_handle(heh).idx()] * x_[heh.idx()];
                    // 同理，考虑对向半边
                    if (!mesh_->is_boundary(opp))
                        weight -= ctg_[opp.idx()] * transforms[mesh_->face_handle(opp).idx()] * x_[opp.idx()];
                    
                    weight_sum += weight;
                    heh = mesh_->next_halfedge_handle(opp);
                } while (heh != heh_start);
                
                b.row(i) = weight_sum;
            }
        });
    
    // 求解全局稀疏线性系统 A * u = b
    u_ = solver_.solve(b);
    if (solver_.info() != Eigen::Success) {
        throw std::runtime_error("ASAP: Failed to solve the linear system.");
    }
    
    // 根据新的 u_ 更新每个面的局部参数化坐标（更新 u_coords）
    tbb::parallel_for(tbb::blocked_range<size_t>(0, mesh_->n_faces()),
        [&](const tbb::blocked_range<size_t>& r) {
            for (size_t i = r.begin(); i != r.end(); ++i) {
                auto fh = mesh_->face_handle(i);
                auto he = mesh_->halfedge_handle(fh);
                auto p0 = mesh_->from_vertex_handle(he);
                auto p1 = mesh_->to_vertex_handle(he);
                auto p2 = mesh_->to_vertex_handle(mesh_->next_halfedge_handle(he));
                
                u_coords_[i].col(0) = u_.row(p1.idx()) - u_.row(p0.idx());
                u_coords_[i].col(1) = u_.row(p2.idx()) - u_.row(p0.idx());
            }
        });
}

USTC_CG_NAMESPACE_CLOSE_SCOPE
