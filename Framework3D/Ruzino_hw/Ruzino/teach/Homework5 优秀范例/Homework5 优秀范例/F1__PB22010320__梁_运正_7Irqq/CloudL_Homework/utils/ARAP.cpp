#include "ARAP.h"
#include <cmath>


USTC_CG_NAMESPACE_OPEN_SCOPE

ARAP::ARAP(std::shared_ptr<PolyMesh> mesh, 
           std::shared_ptr<PolyMesh> param_mesh)
    : mesh_(mesh), 
      param_mesh_(param_mesh ? param_mesh : std::make_shared<PolyMesh>(*mesh)){

    if (!mesh_ || mesh_->n_vertices() == 0) 
    throw std::runtime_error("ARAP: Invalid mesh input.");
    if (!param_mesh_ || param_mesh_->n_vertices() != mesh_->n_vertices()) 
    throw std::runtime_error("ARAP: Invalid or mismatched param_mesh input.");
    
    ctg_.resize(mesh_->n_halfedges(), 0.0f);
    transforms.resize(mesh_->n_faces());
    x_coords_.resize(mesh_->n_faces());
    u_coords_.resize(mesh_->n_faces());
    x_.resize(mesh_->n_halfedges());
}

bool ARAP::needs_precompute(const std::shared_ptr<PolyMesh>& new_mesh, 
    const std::shared_ptr<PolyMesh>& new_param_mesh) const {
    return !precomputed_ || new_mesh != mesh_ || 
    (new_param_mesh != param_mesh_ && new_param_mesh != nullptr);
}

void ARAP::precompute() {
    initialize_solver();
    initialize_param();
    precomputed_ = true;
}

void ARAP::reset() {
    initialize_param();
}

void ARAP::initialize_solver() {
    // 计算余切权重
    for (const auto& heh : mesh_->halfedges()) {
        if (!mesh_->is_boundary(heh)) {
            const auto& v1 = mesh_->point(mesh_->from_vertex_handle(heh)) - mesh_->point
            (mesh_->to_vertex_handle(mesh_->next_halfedge_handle(heh)));
            const auto& v2 = mesh_->point(mesh_->to_vertex_handle(heh)) - mesh_->point
            (mesh_->to_vertex_handle(mesh_->next_halfedge_handle(heh)));
            float cross_norm = v1.cross(v2).norm();
            if (cross_norm < 1e-6f) 
            ctg_[heh.idx()] = 0.0f; 
            else
            ctg_[heh.idx()] = v1.dot(v2) / cross_norm;
        }
    }

    // 初始化系数矩阵 A
    for (const auto& vh : mesh_->vertices()) {
        int i = vh.idx();
        if (i == 0) {
            triplets_.emplace_back(i, i, 1);
            continue;
        }
        float weight_sum = 0.0f;
        for (const auto& heh : vh.outgoing_halfedges()) {
            int j = mesh_->to_vertex_handle(heh).idx();
            auto op_heh = mesh_->opposite_halfedge_handle(heh);
            float weight = ctg_[heh.idx()] + ctg_[op_heh.idx()];
            triplets_.emplace_back(i, j, -weight);
            weight_sum += weight;
        }
        triplets_.emplace_back(i, i, weight_sum);
    }

    A_.resize(mesh_->n_vertices(), mesh_->n_vertices());
    A_.setFromTriplets(triplets_.begin(), triplets_.end());
    if (A_.nonZeros() == 0) 
    throw std::runtime_error("ARAP: Coefficient matrix is empty.");
    solver_.compute(A_);
    if (solver_.info() != Eigen::Success) {
        throw std::runtime_error("Failed to decompose the matrix.");
    }
}

void ARAP::local_phase(){
    // Local phase
    tbb::parallel_for(tbb::blocked_range<size_t>(0, mesh_->n_faces()),
    [&](const tbb::blocked_range<size_t>& r) {
        for (size_t i = r.begin(); i != r.end(); ++i) {
            auto fh = mesh_->face_handle(i);
            Eigen::Matrix2f J = u_coords_[i] * x_coords_[i].inverse();
            Eigen::JacobiSVD<Eigen::Matrix2f> svd(J, Eigen::ComputeFullU | Eigen::ComputeFullV);
            transforms[i] = svd.matrixU() * svd.matrixV().transpose();
        }
    });
}

void ARAP::global_phase(){
    // Global phase
        Eigen::MatrixXf b(mesh_->n_vertices(), 2);
        b.setZero();
        tbb::parallel_for(tbb::blocked_range<size_t>(0, mesh_->n_vertices()),
    [&](const tbb::blocked_range<size_t>& range) {
        for (size_t idx = range.begin(); idx != range.end(); ++idx) {
            auto vh = mesh_->vertex_handle(idx);
            int i = vh.idx();
            
            if (i == 0) {
                b(i, 0) = 0.0f;
                b(i, 1) = 0.0f;
                continue;
            }  // 跳过第 0 个顶点（假设有特殊用途）
            
            Eigen::Vector2f weight_sum = Eigen::Vector2f::Zero();

            // 获取顶点的 outgoing halfedge（起始半边）
            auto heh_start = mesh_->halfedge_handle(vh);
            if (!heh_start.is_valid()) continue;  // 确保顶点有半边

            // 遍历所有 outgoing halfedges
            auto heh = heh_start;
            do {
                // 获取对向半边
                auto opp = mesh_->opposite_halfedge_handle(heh);
                Eigen::Vector2f weight = Eigen::Vector2f::Zero();

                // 计算权重
                if (!mesh_->is_boundary(heh)) 
                    weight += ctg_[heh.idx()] * transforms[mesh_->face_handle(heh).idx()] * x_[heh.idx()];
                
                if (!mesh_->is_boundary(opp)) 
                    weight -= ctg_[opp.idx()] * transforms[mesh_->face_handle(opp).idx()] * x_[opp.idx()];
                
                weight_sum += weight;

                // 移动到下一个 outgoing halfedge
                heh = mesh_->next_halfedge_handle(opp);
            } while (heh != heh_start);  // 循环直到回到起始半边

            b.row(i) = weight_sum;
        }
    });

        u_ = solver_.solve(b);
        // 更新 U
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

void ARAP::ARAP_iteration(){
        local_phase();

        global_phase();
}

void ARAP::execute(int iterations) {
    if (!precomputed_) {
        precompute();
    }

    for (int j = 0; j < iterations; ++j) {
        ARAP_iteration();
    }
}

void ARAP::initialize_param(){
    const size_t n_faces = mesh_->n_faces();
    for (int fi = 0; fi < n_faces; ++fi) {
        const auto fh = mesh_->face_handle(fi);
        const auto he = mesh_->halfedge_handle(fh);  // 正确获取面片的半边
        
        const auto v0 = mesh_->from_vertex_handle(he);
        const auto v1 = mesh_->to_vertex_handle(he);
        const auto v2 = mesh_->to_vertex_handle(mesh_->next_halfedge_handle(he));

        // 计算3D边向量和长度
        const auto e1 = mesh_->point(v1) - mesh_->point(v0);
        const auto e2 = mesh_->point(v2) - mesh_->point(v0);
        const float e1_len = e1.norm();
        const float e2_len = e2.norm();

        // 计算局部坐标系基向量
        const auto e1_dir = e1 / e1_len;
        const float cos_theta = e1_dir.dot(e2) / e2_len;
        const float sin_theta = e1_dir.cross(e2).norm() / e2_len;

        // 设置半边向量
        x_[he.idx()] = {-e1_len, 0.0f};
        x_[mesh_->next_halfedge_handle(he).idx()] = {e1_len - e2_len * cos_theta, -e2_len * sin_theta};
        x_[mesh_->next_halfedge_handle(mesh_->next_halfedge_handle(he)).idx()] = {e2_len * cos_theta, e2_len * sin_theta};

        // 初始化坐标矩阵
        x_coords_[fi] << e1_len, e2_len * cos_theta, 
                          0.0f,   e2_len * sin_theta;
        u_coords_[fi] << (param_mesh_->point(v1) - param_mesh_->point(v0))[0],
                         (param_mesh_->point(v2) - param_mesh_->point(v0))[0],
                         (param_mesh_->point(v1) - param_mesh_->point(v0))[1],
                         (param_mesh_->point(v2) - param_mesh_->point(v0))[1];
    }
}

pxr::VtArray<pxr::GfVec2f> ARAP::get_result() const {
    pxr::VtArray<pxr::GfVec2f> uv_result(mesh_->n_vertices());
    float min_u = FLT_MAX, max_u = -FLT_MAX;
    float min_v = FLT_MAX, max_v = -FLT_MAX;
    for (int i = 0; i < mesh_->n_vertices(); ++i) {
        min_u = std::min(min_u, u_(i, 0));
        max_u = std::max(max_u, u_(i, 0));
        min_v = std::min(min_v, u_(i, 1));
        max_v = std::max(max_v, u_(i, 1));
    }
    
    // 归一化到[0,1]范围
    
    float scale = 1.0f / std::max(max_u - min_u, max_v - min_v);
    for (int i = 0; i < mesh_->n_vertices(); ++i) {
        uv_result[i] = {
            (u_(i, 0) - min_u) * scale,
            (u_(i, 1) - min_v) * scale
        };
    }
    return uv_result;
}
USTC_CG_NAMESPACE_CLOSE_SCOPE