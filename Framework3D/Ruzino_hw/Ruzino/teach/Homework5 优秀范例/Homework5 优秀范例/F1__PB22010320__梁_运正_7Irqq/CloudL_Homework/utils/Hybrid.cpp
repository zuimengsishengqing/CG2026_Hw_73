#include "Hybrid.h"
#include <cmath>

USTC_CG_NAMESPACE_OPEN_SCOPE

Hybrid::Hybrid(
    std::shared_ptr<PolyMesh> mesh,
    std::shared_ptr<PolyMesh> param_mesh,
    float lambda
) : ARAP(mesh, param_mesh), lambda_(lambda) {}

float Hybrid::newton_method(float alpha, float beta, float gamma, float a_init){
    float a = a_init;
    for (int iter = 0; iter < 100; ++iter) {
        float f = alpha * a * a * a + beta * a + gamma;
        float df = 3 * alpha * a * a + beta;
        if (std::abs(df) < 1e-6f) break;
        float delta = f / df;
        a -= delta;
        if (std::abs(delta) < 1e-6f) break;
    }
    return a;
}

void Hybrid::local_phase() {
    tbb::parallel_for(tbb::blocked_range<size_t>(0, mesh_->n_faces()),
    [&](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i < r.end(); ++i) {
            auto fh = mesh_->face_handle(i);
            auto he = mesh_->halfedge_handle(fh);
            
            // 获取三个顶点
            auto v0 = mesh_->from_vertex_handle(he);
            auto v1 = mesh_->to_vertex_handle(he);
            auto v2 = mesh_->to_vertex_handle(mesh_->next_halfedge_handle(he));
            
            // 获取三条边的余切权重（注意半边方向）
            float w0 = ctg_[mesh_->next_halfedge_handle(he).idx()];
            float w1 = ctg_[he.idx()];
            float w2 = ctg_[mesh_->prev_halfedge_handle(he).idx()];
            
            // 3D边向量（来自x_coords_）
            Eigen::Vector2f e0 = x_coords_[i].col(1) - x_coords_[i].col(0); // v2-v1
            Eigen::Vector2f e1 = x_coords_[i].col(0);                      // v1-v0
            Eigen::Vector2f e2 = x_coords_[i].col(1);                      // v2-v0
            
            // 2D边向量（来自u_coords_）
            Eigen::Vector2f du0 = u_coords_[i].col(1) - u_coords_[i].col(0); // u2-u1
            Eigen::Vector2f du1 = u_coords_[i].col(0);                      // u1-u0
            Eigen::Vector2f du2 = u_coords_[i].col(1);                      // u2-u0
            
            // 正确计算系数（完全按照论文定义）
            float C1 = w0 * e0.squaredNorm() + w1 * e1.squaredNorm() + w2 * e2.squaredNorm();
            float C2 = w0 * du0.dot(e0) + w1 * du1.dot(e1) + w2 * du2.dot(e2);
            float C3 = w0 * (du0.x()*e0.y() - du0.y()*e0.x()) + 
                       w1 * (du1.x()*e1.y() - du1.y()*e1.x()) + 
                       w2 * (du2.x()*e2.y() - du2.y()*e2.x());
            
            // 构造三次方程（论文方程B3）
            float alpha = 2.0f * lambda_ * (C2*C2 + C3*C3) / (C2*C2);
            float beta = C1 - 2.0f * lambda_;
            float gamma = -C2;
            
            // 牛顿迭代求解
            float a_init = C2 / std::sqrt(C2*C2 + C3*C3);
            float a = newton_method(alpha, beta, gamma, a_init);
            float b = (C3 / C2) * a;
            
            transforms[i] << a, b, -b, a;
        }
    });
}

USTC_CG_NAMESPACE_CLOSE_SCOPE