// utils/min_surf_cot.cpp
#include "min_surf_cot.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

MinSurfCot::MinSurfCot(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original) 
    : TutteParameterizer(mesh,original) {}

double MinSurfCot::compute_cotangent(const OpenMesh::Vec3f& p0, const OpenMesh::Vec3f& p1, const OpenMesh::Vec3f& p2) {
    OpenMesh::Vec3f a = p0 - p2;
    OpenMesh::Vec3f b = p1 - p2;
    double cos_alpha = a | b;
    double sin_alpha = (a % b).norm();
    return (sin_alpha < 1e-6) ? 0.1 : cos_alpha / sin_alpha;
}

void MinSurfCot::execute() {
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
            for (auto heh : vh.outgoing_halfedges()) {
                int j = mesh_->to_vertex_handle(heh).idx();
                double weight = 0.0;
                auto heh_opp = mesh_->opposite_halfedge_handle(heh);
                if (!mesh_->is_boundary(heh)) {
                    auto heh_next = mesh_->next_halfedge_handle(heh);
                    auto vk = original_mesh_->point(mesh_->to_vertex_handle(heh_next));
                    weight += compute_cotangent(original_mesh_->point(vh), original_mesh_->point(mesh_->vertex_handle(j)), vk);
                }
                if (!mesh_->is_boundary(heh_opp)) {
                    auto heh_opp_next = mesh_->next_halfedge_handle(heh_opp);
                    auto vl = original_mesh_->point(mesh_->to_vertex_handle(heh_opp_next));
                    weight += compute_cotangent(pos, original_mesh_->point(original_mesh_->vertex_handle(j)), vl);
                }
                if (weight < 0 || std::isnan(weight) || std::isinf(weight)) weight = 0.1;
                if (weight > 10.0) weight = 10.0;
                neighbors.push_back({j, weight});
                weight_sum += weight;
            }
            if (weight_sum < 1e-6) weight_sum = 1.0;
            triplets.push_back(Eigen::Triplet<double>(i, i, weight_sum));
            for (const auto& [j, w] : neighbors) {
                triplets.push_back(Eigen::Triplet<double>(i, j, -w));
            }
        }
    }

    Eigen::SparseMatrix<double> L(n_vertices, n_vertices);
    L.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success) throw std::runtime_error("SparseLU failed!");

    Eigen::VectorXd x = solver.solve(bx);
    Eigen::VectorXd y = solver.solve(by);
    Eigen::VectorXd z = solver.solve(bz);

    update_mesh(x, y, z);
}

USTC_CG_NAMESPACE_CLOSE_SCOPE