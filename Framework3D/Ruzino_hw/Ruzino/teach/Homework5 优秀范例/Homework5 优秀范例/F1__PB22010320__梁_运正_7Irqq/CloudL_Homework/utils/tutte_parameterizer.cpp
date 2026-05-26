// utils/tutte_parameterizer.cpp
#include "tutte_parameterizer.h"
#include <algorithm>

USTC_CG_NAMESPACE_OPEN_SCOPE

TutteParameterizer::TutteParameterizer(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original)
    : mesh_(mesh) {
    if (!mesh_ || mesh_->n_vertices() == 0) {
        throw std::runtime_error("TutteParameterizer: Invalid mesh input.");
    }
    
    // 如果提供了原始网格，使用它；否则深拷贝当前网格
    original_mesh_ = original ? original : std::make_shared<PolyMesh>(*mesh_);

    if (!original_mesh_) {
        throw std::runtime_error("TutteParameterizer: Failed to initialize original_mesh_.");
    }
    detect_boundaries();
}

std::shared_ptr<Geometry> TutteParameterizer::get_result() const {
    return openmesh_to_operand(mesh_.get());
}

void TutteParameterizer::detect_boundaries() {
    int n_vertices = mesh_->n_vertices();
    is_boundary_.resize(n_vertices, false);
    boundary_indices_.clear();

    auto start_heh = *std::find_if(mesh_->halfedges().begin(), mesh_->halfedges().end(),
                                   [this](const auto& heh) { return mesh_->is_boundary(heh); });
    int start_idx = mesh_->from_vertex_handle(start_heh).idx();
    boundary_indices_.push_back(start_idx);
    is_boundary_[start_idx] = true;

    auto heh = start_heh;
    do {
        heh = mesh_->next_halfedge_handle(heh);
        while (!mesh_->is_boundary(heh)) {
            heh = mesh_->opposite_halfedge_handle(mesh_->next_halfedge_handle(heh));
        }
        int next_idx = mesh_->to_vertex_handle(heh).idx();
        if (!is_boundary_[next_idx]) {
            boundary_indices_.push_back(next_idx);
            is_boundary_[next_idx] = true;
        }
    } while (heh != start_heh && boundary_indices_.size() < n_vertices);
}

void TutteParameterizer::update_mesh(const Eigen::VectorXd& x, const Eigen::VectorXd& y, const Eigen::VectorXd& z) {
    for (auto vh : mesh_->vertices()) {
        int i = vh.idx();
        mesh_->point(vh) = OpenMesh::Vec3f(float(x(i)), float(y(i)), float(z(i)));
    }
}

USTC_CG_NAMESPACE_CLOSE_SCOPE