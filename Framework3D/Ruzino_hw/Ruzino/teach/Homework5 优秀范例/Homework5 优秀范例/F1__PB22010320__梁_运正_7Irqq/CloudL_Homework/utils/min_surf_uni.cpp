// utils/min_surf_uni.cpp
#include "min_surf_uni.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

MinSurfUni::MinSurfUni(std::shared_ptr<PolyMesh> mesh) : TutteParameterizer(mesh) {}

void MinSurfUni::execute() {
    int n_vertices = mesh_->n_vertices();
    std::vector<Eigen::Triplet<double>> triplets;
    Eigen::VectorXd bx(n_vertices), by(n_vertices), bz(n_vertices);

    for (auto vh : mesh_->vertices()) {
        int i = vh.idx();
        auto pos = mesh_->point(vh);
        if (is_boundary_[i]) {
            triplets.push_back(Eigen::Triplet<double>(i, i, 1.0));
            bx[i] = pos[0];
            by[i] = pos[1];
            bz[i] = pos[2];
        } else {
            int degree = 0;
            for (auto voh : vh.outgoing_halfedges()) degree++;
            triplets.push_back(Eigen::Triplet<double>(i, i, degree));
            for (auto voh : vh.outgoing_halfedges()) {
                int j = mesh_->to_vertex_handle(voh).idx();
                triplets.push_back(Eigen::Triplet<double>(i, j, -1.0));
            }
            bx[i] = 0.0;
            by[i] = 0.0;
            bz[i] = 0.0;
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