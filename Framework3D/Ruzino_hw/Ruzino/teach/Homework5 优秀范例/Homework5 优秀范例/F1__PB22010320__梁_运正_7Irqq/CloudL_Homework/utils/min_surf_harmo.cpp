// utils/min_surf_harmo.cpp
#include "min_surf_harmo.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

MinSurfHarmo::MinSurfHarmo(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original) 
    : TutteParameterizer(mesh,original) {}

double MinSurfHarmo::compute_harmo_weight(int vi, int vj) {
    auto pi = original_mesh_->point(mesh_->vertex_handle(vi));
    auto pj = original_mesh_->point(mesh_->vertex_handle(vj));
    double dist = (pi - pj).norm();
    return dist > 1e-6 ? 1.0 / dist : 0.1;  // 示例：基于距离倒数的权重
}

void MinSurfHarmo::execute() {
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
            for (auto voh : vh.outgoing_halfedges()) {
                int j = mesh_->to_vertex_handle(voh).idx();
                double weight = compute_harmo_weight(i, j);
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