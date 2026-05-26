#include "FastMassSpring.h"

#include <iostream>
#include <vector>
#include <omp.h>

namespace USTC_CG::mass_spring {

FastMassSpring::FastMassSpring(
    const Eigen::MatrixXd& X,
    const EdgeSet& E,
    const float stiffness,
    const float h,
    const bool volumetric)
{
    this->X = this->init_X = X;
    this->vel = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    this->stiffness = stiffness;
    this->h = h;
    this->volumetric = volumetric;

    // If volumetric is enabled, we will build an internal edge set assuming a
    // structured cubic grid when possible. Otherwise use provided edge set.
    if (this->volumetric) {
        // start with the provided E as a hint but we will overwrite
        this->E.clear();
    } else {
        this->E = E;
    }

    std::cout << "init fast mass spring" << std::endl;

    // If volumetric, attempt to create volumetric springs
    if (this->volumetric) {
        buildVolumetricSpringsIfNeeded();
    }

    // Compute the rest pose edge length
    for (const auto& e : this->E) {
        Eigen::Vector3d x0 = X.row(e.first);
        Eigen::Vector3d x1 = X.row(e.second);
        this->E_rest_length.push_back((x0 - x1).norm());
    }

    // Initialize the mask for Dirichlet boundary condition
    dirichlet_bc_mask.resize(X.rows(), false);

    if (this->volumetric && grid_nx > 0) {
        // Fix all vertices on the x=0 vertical face
        // This allows us to observe the deformation of other faces
        for (int vi = 0; vi < X.rows(); ++vi) {
            if (std::abs(X(vi, 0) - 0.0) < 1e-6) {
                dirichlet_bc_mask[vi] = true;
            }
        }
    } else {
        // (HW_TODO) Fix two vertices for cloth, feel free to modify this
        unsigned n_fix = static_cast<unsigned>(std::sqrt(X.rows()));  // Here we assume the cloth is square
        if (X.rows() > 0) {
            dirichlet_bc_mask[0] = true;
            if (n_fix > 0 && n_fix - 1 < dirichlet_bc_mask.size())
                dirichlet_bc_mask[n_fix - 1] = true;
        }
    }
    
    // Debug: print fixed vertices
    std::cout << "Total vertices: " << X.rows() << ", volumetric: " << this->volumetric << std::endl;
    std::cout << "Fixed vertices:" << std::endl;
    for (unsigned vi = 0; vi < dirichlet_bc_mask.size(); vi++) {
        if (dirichlet_bc_mask[vi]) {
            std::cout << "  Vertex " << vi << ": [" << X(vi, 0) << ", " << X(vi, 1) << ", " << X(vi, 2) << "]" << std::endl;
        }
    }
}

void FastMassSpring::buildVolumetricSpringsIfNeeded()
{
    if (!volumetric) return;
    const int n = static_cast<int>(X.rows());
    
    // Determine grid dimensions by analyzing unique coordinate values
    // This is more reliable than brute force factorization
    std::set<double> unique_x, unique_y, unique_z;
    
    for (int i = 0; i < n; ++i) {
        unique_x.insert(X(i, 0));
        unique_y.insert(X(i, 1));
        unique_z.insert(X(i, 2));
    }
    
    grid_nx = static_cast<int>(unique_x.size());
    grid_ny = static_cast<int>(unique_y.size());
    grid_nz = static_cast<int>(unique_z.size());
    
    // Verify that the dimensions match the vertex count
    if (grid_nx * grid_ny * grid_nz != n) {
        std::cerr << "FastMassSpring: volumetric mode but vertex count (" << n 
                  << ") does not match grid dimensions (" << grid_nx << "*" << grid_ny << "*" << grid_nz 
                  << " = " << (grid_nx * grid_ny * grid_nz) << "). "
                  << "Fallback: no volumetric springs built." << std::endl;
        grid_nx = grid_ny = grid_nz = 0;
        return;
    }
    
    std::cout << "FastMassSpring: found grid dimensions = (" << grid_nx << "," << grid_ny << "," << grid_nz << ") for " << n << " vertices" << std::endl;

    // Define index mapping that matches USDA data layout:
    // z-axis changes fastest (inner loop), y-axis second, x-axis slowest (outer loop)
    auto get_idx = [&](int i, int j, int k) {
        return i * (grid_ny * grid_nz) + j * grid_nz + k;
    };

    // Build complete volumetric spring topology with correct indexing
    for (int i = 0; i < grid_nx; ++i) {
        for (int j = 0; j < grid_ny; ++j) {
            for (int k = 0; k < grid_nz; ++k) {
                int idx = get_idx(i, j, k);
                
                // Structural springs (axis-aligned) - maintain basic shape
                if (i + 1 < grid_nx) {
                    int idx2 = get_idx(i + 1, j, k);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                }
                if (j + 1 < grid_ny) {
                    int idx2 = get_idx(i, j + 1, k);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                }
                if (k + 1 < grid_nz) {
                    int idx2 = get_idx(i, j, k + 1);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                }
                
                // Shear springs (face diagonals) - resist shear deformation
                // XY plane
                if (i + 1 < grid_nx && j + 1 < grid_ny) {
                    int idx2 = get_idx(i + 1, j + 1, k);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                    
                    int idx3 = get_idx(i, j + 1, k);
                    int idx4 = get_idx(i + 1, j, k);
                    E.insert(std::make_pair(std::min(idx3, idx4), std::max(idx3, idx4)));
                }
                // XZ plane
                if (i + 1 < grid_nx && k + 1 < grid_nz) {
                    int idx2 = get_idx(i + 1, j, k + 1);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                    
                    int idx3 = get_idx(i, j, k + 1);
                    int idx4 = get_idx(i + 1, j, k);
                    E.insert(std::make_pair(std::min(idx3, idx4), std::max(idx3, idx4)));
                }
                // YZ plane
                if (j + 1 < grid_ny && k + 1 < grid_nz) {
                    int idx2 = get_idx(i, j + 1, k + 1);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                    
                    int idx3 = get_idx(i, j, k + 1);
                    int idx4 = get_idx(i, j + 1, k);
                    E.insert(std::make_pair(std::min(idx3, idx4), std::max(idx3, idx4)));
                }
                
                // Volume springs (body diagonals) - maintain volume, prevent collapse
                if (i + 1 < grid_nx && j + 1 < grid_ny && k + 1 < grid_nz) {
                    int idx2 = get_idx(i + 1, j + 1, k + 1);
                    E.insert(std::make_pair(std::min(idx, idx2), std::max(idx, idx2)));
                    
                    int idx3 = get_idx(i + 1, j, k);
                    int idx4 = get_idx(i, j + 1, k + 1);
                    E.insert(std::make_pair(std::min(idx3, idx4), std::max(idx3, idx4)));
                    
                    int idx5 = get_idx(i, j + 1, k);
                    int idx6 = get_idx(i + 1, j, k + 1);
                    E.insert(std::make_pair(std::min(idx5, idx6), std::max(idx5, idx6)));
                    
                    int idx7 = get_idx(i, j, k + 1);
                    int idx8 = get_idx(i + 1, j + 1, k);
                    E.insert(std::make_pair(std::min(idx7, idx8), std::max(idx7, idx8)));
                }
            }
        }
    }
    std::cout << "FastMassSpring: built volumetric springs, vertex grid = (" << grid_nx << "," << grid_ny << "," << grid_nz << "), edges = " << E.size() << std::endl;
}

static inline Eigen::VectorXd flattenMatrixToVector(const Eigen::MatrixXd& M)
{
    Eigen::VectorXd v(M.rows() * M.cols());
    for (int i = 0; i < M.rows(); i++) {
        for (int j = 0; j < M.cols(); j++) {
            v(i * M.cols() + j) = M(i, j);
        }
    }
    return v;
}

static inline Eigen::MatrixXd unflattenVectorToMatrix(
    const Eigen::VectorXd& v,
    int rows,
    int cols)
{
    Eigen::MatrixXd M(rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            M(i, j) = v(i * cols + j);
        }
    }
    return M;
}

void FastMassSpring::buildSystemMatrix()
{
    n_vertices = X.rows();
    const int n_dofs = static_cast<int>(n_vertices * 3);
    const double mass_per_vertex = mass / static_cast<double>(n_vertices);
    const double k = stiffness;

    std::vector<Trip_d> triplets;
    triplets.reserve(n_vertices * 3 + E.size() * 12);

    // Mass term: diagonal entries
    for (int vi = 0; vi < n_vertices; vi++) {
        for (int j = 0; j < 3; j++) {
            int idx = vi * 3 + j;
            triplets.emplace_back(idx, idx, mass_per_vertex);
        }
    }

    // Stiffness Laplacian term: M + h^2 * L
    // Directly multiply by h^2 when inserting to avoid fragile post-processing
    double h2_stiffness = k * (h * h);
    for (const auto& e : E) {
        int i = e.first;
        int j = e.second;
        for (int d = 0; d < 3; d++) {
            int row_i = i * 3 + d;
            int row_j = j * 3 + d;
            triplets.emplace_back(row_i, row_i, h2_stiffness);
            triplets.emplace_back(row_j, row_j, h2_stiffness);
            triplets.emplace_back(row_i, row_j, -h2_stiffness);
            triplets.emplace_back(row_j, row_i, -h2_stiffness);
        }
    }

    // Apply Dirichlet boundary condition
    // Completely remove all non-diagonal entries involving fixed DOFs
    std::vector<Trip_d> corrected;
    corrected.reserve(triplets.size());
    
    for (const auto& t : triplets) {
        int row_vertex = t.row() / 3;
        int col_vertex = t.col() / 3;
        bool row_fixed = row_vertex >= 0 && row_vertex < dirichlet_bc_mask.size() &&
                         dirichlet_bc_mask[row_vertex];
        bool col_fixed = col_vertex >= 0 && col_vertex < dirichlet_bc_mask.size() &&
                         dirichlet_bc_mask[col_vertex];

        // Skip any entry involving fixed DOFs (completely zero out)
        if (row_fixed || col_fixed) {
            continue; 
        }
        
        // Only keep entries where both row and col are free
        corrected.emplace_back(t);
    }

    // Add perfect 1.0 diagonal for all fixed DOFs
    for (int vi = 0; vi < n_vertices; vi++) {
        if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
            for (int j = 0; j < 3; j++) {
                int idx = vi * 3 + j;
                corrected.emplace_back(idx, idx, 1.0);
            }
        }
    }

    A.resize(n_dofs, n_dofs);
    A.setFromTriplets(corrected.begin(), corrected.end());
    A.makeCompressed();

    A_solver.compute(A);
    if (A_solver.info() != Eigen::Success) {
        std::cerr << "FastMassSpring: pre-factorization failed. "
                  << "Try reducing stiffness or time step." << std::endl;
        A_prefactored = false;
    }
    else {
        A_prefactored = true;
        prefactor_stiffness = stiffness;
        prefactor_h = h;
    }
}

void FastMassSpring::step()
{
    if (!A_prefactored || stiffness != prefactor_stiffness || h != prefactor_h) {
        buildSystemMatrix();
    }
    if (!A_prefactored) {
        std::cerr << "FastMassSpring: solver not ready, aborting step." << std::endl;
        return;
    }

    static bool first_step = true;
    if (first_step) {
        std::cout << "FastMassSpring: step() called with:" << std::endl;
        std::cout << "  stiffness = " << stiffness << std::endl;
        std::cout << "  h = " << h << std::endl;
        std::cout << "  mass = " << mass << std::endl;
        std::cout << "  gravity = [" << gravity[0] << ", " << gravity[1] << ", " << gravity[2] << "]" << std::endl;
        std::cout << "  wind_ext_acc = [" << wind_ext_acc[0] << ", " << wind_ext_acc[1] << ", " << wind_ext_acc[2] << "]" << std::endl;
        std::cout << "  damping = " << damping << std::endl;
        std::cout << "  max_iter = " << max_iter << std::endl;
        std::cout << "  enable_damping = " << enable_damping << std::endl;
        first_step = false;
    }

    unsigned n_vertices = X.rows();
    const double mass_per_vertex = mass / static_cast<double>(n_vertices);
    const double h2 = h * h;

    Eigen::Vector3d acceleration_ext = gravity + wind_ext_acc;
    Eigen::MatrixXd Y = X + h * vel;
    Y.rowwise() += (h2 * acceleration_ext).transpose();
    
    static int step_count = 0;
    Eigen::MatrixXd X_before = X;
    
    // NOTE: Collision is handled via projection in each iteration, not in Y
    // This follows the paper's approach: "directly move x to the desired collision-free state"

    std::vector<Eigen::Vector3d> d(E.size());
    std::vector<Eigen::Vector3d> fixed_positions(n_vertices);
    for (unsigned vi = 0; vi < n_vertices; vi++) {
        if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
            fixed_positions[vi] = X.row(vi).transpose();
        }
    }

    Eigen::MatrixXd X_old = X;
    for (unsigned iter = 0; iter < max_iter; iter++) {
        // Local step: compute target spring directions
        unsigned ei = 0;
        if (enable_omp_parallel) {
            #pragma omp parallel for schedule(dynamic)
            for (int eidx = 0; eidx < static_cast<int>(E.size()); eidx++) {
                auto it = E.begin();
                std::advance(it, eidx);
                const auto& e = *it;
                Eigen::Vector3d edge_vector =
                    X.row(e.first).transpose() - X.row(e.second).transpose();
                double current_length = edge_vector.norm();
                double rest_length = E_rest_length[eidx];
                if (current_length > 1e-12) {
                    d[eidx] = rest_length * edge_vector / current_length;
                }
                else {
                    d[eidx] = Eigen::Vector3d(rest_length, 0.0, 0.0);
                }
            }
        } else {
            for (const auto& e : E) {
                Eigen::Vector3d edge_vector =
                    X.row(e.first).transpose() - X.row(e.second).transpose();
                double current_length = edge_vector.norm();
                double rest_length = E_rest_length[ei];
                if (current_length > 1e-12) {
                    d[ei] = rest_length * edge_vector / current_length;
                }
                else {
                    d[ei] = Eigen::Vector3d(rest_length, 0.0, 0.0);
                }
                ei++;
            }
        }

        // Global step: solve A x = b
        Eigen::VectorXd b = Eigen::VectorXd::Zero(n_vertices * 3);
        
        if (enable_omp_parallel) {
            #pragma omp parallel for schedule(static)
            for (int vi = 0; vi < static_cast<int>(n_vertices); vi++) {
                for (int didx = 0; didx < 3; didx++) {
                    b(vi * 3 + didx) = mass_per_vertex * Y(vi, didx);
                }
            }
        } else {
            for (int vi = 0; vi < n_vertices; vi++) {
                for (int didx = 0; didx < 3; didx++) {
                    b(vi * 3 + didx) = mass_per_vertex * Y(vi, didx);
                }
            }
        }

        // Add spring force contributions
        if (enable_omp_parallel) {
            #pragma omp parallel for schedule(dynamic)
            for (int eidx = 0; eidx < static_cast<int>(E.size()); eidx++) {
                auto it = E.begin();
                std::advance(it, eidx);
                const auto& e = *it;
                const Eigen::Vector3d& d_i = d[eidx];
                Eigen::Vector3d contribution = stiffness * d_i;
                for (int didx = 0; didx < 3; didx++) {
                    #pragma omp atomic
                    b(e.first * 3 + didx) += h2 * contribution[didx];
                    #pragma omp atomic
                    b(e.second * 3 + didx) -= h2 * contribution[didx];
                }
            }
        } else {
            unsigned edge_index = 0;
            for (const auto& e : E) {
                const Eigen::Vector3d& d_i = d[edge_index];
                Eigen::Vector3d contribution = stiffness * d_i;
                for (int didx = 0; didx < 3; didx++) {
                    b(e.first * 3 + didx) += h2 * contribution[didx];
                    b(e.second * 3 + didx) -= h2 * contribution[didx];
                }
                edge_index++;
            }
        }

        // Compensate for fixed DOF contributions by directly iterating edges
        // This replaces the fragile InnerIterator approach
        for (const auto& e : E) {
            int i = e.first;
            int j = e.second;
            bool i_fixed = i < dirichlet_bc_mask.size() && dirichlet_bc_mask[i];
            bool j_fixed = j < dirichlet_bc_mask.size() && dirichlet_bc_mask[j];

            // Case A: i is free, j is fixed. Add j's contribution to rhs
            if (!i_fixed && j_fixed) {
                for (int didx = 0; didx < 3; didx++) {
                    b(i * 3 + didx) += h2 * stiffness * fixed_positions[j][didx];
                }
            }
            // Case B: i is fixed, j is free. Add i's contribution to rhs
            else if (i_fixed && !j_fixed) {
                for (int didx = 0; didx < 3; didx++) {
                    b(j * 3 + didx) += h2 * stiffness * fixed_positions[i][didx];
                }
            }
        }

        // Set rhs for fixed DOFs
        for (int vi = 0; vi < n_vertices; vi++) {
            if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
                for (int didx = 0; didx < 3; didx++) {
                    b(vi * 3 + didx) = fixed_positions[vi][didx];
                }
            }
        }

        Eigen::VectorXd x_flat = A_solver.solve(b);
        if (A_solver.info() != Eigen::Success) {
            std::cerr << "FastMassSpring: solve failed during iteration " << iter
                      << "." << std::endl;
            return;
        }

        Eigen::MatrixXd X_new = unflattenVectorToMatrix(x_flat, n_vertices, 3);

        for (int vi = 0; vi < n_vertices; vi++) {
            if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
                X_new.row(vi) = fixed_positions[vi].transpose();
            }
        }

        // Apply collision projection (as suggested in the paper)
        if (enable_sphere_collision) {
            double collision_dist = collision_scale_factor * sphere_radius;
            if (enable_omp_parallel) {
                #pragma omp parallel for schedule(static)
                for (int vi = 0; vi < static_cast<int>(n_vertices); vi++) {
                    if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
                        continue;  // Skip fixed vertices
                    }
                    
                    Eigen::Vector3d x = X_new.row(vi).transpose();
                    Eigen::Vector3d diff = x - sphere_center.cast<double>();
                    double dist = diff.norm();
                    
                    if (dist < collision_dist && dist > 1e-10) {
                        // Project vertex outside the collision sphere
                        Eigen::Vector3d normal = diff / dist;
                        X_new.row(vi) = (sphere_center.cast<double>() + normal * collision_dist).transpose();
                    }
                }
            } else {
                for (int vi = 0; vi < n_vertices; vi++) {
                    if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
                        continue;  // Skip fixed vertices
                    }
                    
                    Eigen::Vector3d x = X_new.row(vi).transpose();
                    Eigen::Vector3d diff = x - sphere_center.cast<double>();
                    double dist = diff.norm();
                    
                    if (dist < collision_dist && dist > 1e-10) {
                        // Project vertex outside the collision sphere
                        Eigen::Vector3d normal = diff / dist;
                        X_new.row(vi) = (sphere_center.cast<double>() + normal * collision_dist).transpose();
                    }
                }
            }
        }

        X = X_new;
    }

    if (h > 0.0) {
        vel = (X - X_old) / h;
    }
    else {
        vel.setZero();
    }
    
    if (step_count < 3) {
        std::cout << "FastMassSpring: after step " << step_count << ":" << std::endl;
        std::cout << "  Sample free vertex positions:" << std::endl;
        int sample_count = 0;
        for (int vi = 0; vi < n_vertices && sample_count < 5; ++vi) {
            if (vi < dirichlet_bc_mask.size() && !dirichlet_bc_mask[vi]) {
                Eigen::Vector3d delta = X.row(vi) - X_before.row(vi);
                std::cout << "    Vertex " << vi << ": [" << X(vi, 0) << ", " << X(vi, 1) << ", " << X(vi, 2) << "]";
                std::cout << ", delta = [" << delta[0] << ", " << delta[1] << ", " << delta[2] << "]" << std::endl;
                sample_count++;
            }
        }
        step_count++;
    }

    if (enable_damping) {
        vel *= damping;
    }

    for (int vi = 0; vi < n_vertices; vi++) {
        if (vi < dirichlet_bc_mask.size() && dirichlet_bc_mask[vi]) {
            vel.row(vi).setZero();
        }
    }

    current_time += h;
    step_count++;

    if (current_time >= next_print_time) {
        std::cout << "\n=== Quantitative Analysis at t=" << current_time << "s (step " << step_count << ") ===" << std::endl;
        std::cout << "Params: k=" << stiffness << ", h=" << h << ", m=" << mass << ", damping=" << damping << ", iter=" << max_iter << std::endl;
        std::cout << "Energy: " << computeTotalEnergy() << std::endl;
        std::cout << "MaxVel: " << computeMaxVelocity() << std::endl;
        std::cout << "MaxDef: " << computeMaxDeformation() << std::endl;
        std::cout << "MaxStrain: " << computeMaxStrain() << std::endl;
        std::cout << "StiffnessRatio: " << (stiffness * h * h / (mass / n_vertices)) << std::endl;
        std::cout << "========================================\n" << std::endl;
        next_print_time += print_interval;
    }
}

double FastMassSpring::computeTotalEnergy()
{
    const double mass_per_vertex = mass / static_cast<double>(n_vertices);
    
    double kinetic = 0.5 * mass_per_vertex * vel.squaredNorm();
    
    double potential = 0.0;
    for (const auto& e : E) {
        Eigen::Vector3d edge = X.row(e.first) - X.row(e.second);
        double current_length = edge.norm();
        double rest_length = E_rest_length[&e - &*E.begin()];
        double strain = current_length - rest_length;
        potential += 0.5 * stiffness * strain * strain;
    }
    
    double gravitational = 0.0;
    for (int i = 0; i < n_vertices; ++i) {
        Eigen::Vector3d pos = X.row(i);
        gravitational += mass_per_vertex * gravity.dot(pos);
    }
    
    return kinetic + potential + gravitational;
}

double FastMassSpring::computeMaxVelocity()
{
    return vel.cwiseAbs().maxCoeff();
}

double FastMassSpring::computeMaxDeformation()
{
    double max_def = 0.0;
    for (const auto& e : E) {
        Eigen::Vector3d edge = X.row(e.first) - X.row(e.second);
        double current_length = edge.norm();
        double rest_length = E_rest_length[&e - &*E.begin()];
        double def = std::abs(current_length - rest_length);
        max_def = std::max(max_def, def);
    }
    return max_def;
}

double FastMassSpring::computeMaxStrain()
{
    double max_strain = 0.0;
    for (const auto& e : E) {
        Eigen::Vector3d edge = X.row(e.first) - X.row(e.second);
        double current_length = edge.norm();
        double rest_length = E_rest_length[&e - &*E.begin()];
        if (rest_length > 1e-10) {
            double strain = std::abs(current_length - rest_length) / rest_length;
            max_strain = std::max(max_strain, strain);
        }
    }
    return max_strain;
}

}  // namespace USTC_CG::mass_spring