#include "MassSpring.h"

#include <iostream>

namespace USTC_CG::mass_spring {
MassSpring::MassSpring(const Eigen::MatrixXd& X, const EdgeSet& E)
{
    this->X = this->init_X = X;
    this->vel = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    this->E = E;

    std::cout << "number of edges: " << E.size() << std::endl;
    std::cout << "init mass spring" << std::endl;

    // Compute the rest pose edge length
    for (const auto& e : E) {
        Eigen::Vector3d x0 = X.row(e.first);
        Eigen::Vector3d x1 = X.row(e.second);
        this->E_rest_length.push_back((x0 - x1).norm());
    }

    // Initialize the mask for Dirichlet boundary condition
    dirichlet_bc_mask.resize(X.rows(), false);

    // (HW_TODO) Fix two vertices, feel free to modify this
    unsigned n_fix = sqrt(X.rows());  // Here we assume the cloth is square
    dirichlet_bc_mask[0] = true;
    dirichlet_bc_mask[n_fix - 1] = true;
}

void MassSpring::step()
{
    // Force disable debug output to keep only quantitative analysis
    enable_debug_output = false;
    
    Eigen::Vector3d acceleration_ext = gravity + wind_ext_acc;

    unsigned n_vertices = X.rows();

    // The reason to not use 1.0 as mass per vertex: the cloth gets heavier as
    // we increase the resolution
    double mass_per_vertex = mass / n_vertices;

    //----------------------------------------------------
    // (HW Optional) Bonus part: Sphere collision
    Eigen::MatrixXd acceleration_collision =
        getSphereCollisionForce(sphere_center.cast<double>(), sphere_radius);
    //----------------------------------------------------

    if (time_integrator == IMPLICIT_EULER) {
        // Implicit Euler
        TIC(step)

        if (enable_debug_output) {
            std::cout << "\n=== IMPLICIT EULER STEP START ===" << std::endl;
            std::cout << "Parameters: h=" << h << ", stiffness=" << stiffness 
                      << ", mass=" << mass << ", n_vertices=" << n_vertices << std::endl;
            std::cout << "X range: [" << X.minCoeff() << ", " << X.maxCoeff() << "]" << std::endl;
            std::cout << "vel range: [" << vel.minCoeff() << ", " << vel.maxCoeff() << "]" << std::endl;
            std::cout << "acceleration_ext: " << acceleration_ext.transpose() << std::endl;
        }

        // (HW TODO)
        auto H_elastic = computeHessianSparse(stiffness);  // size = [nx3, nx3]

        if (enable_debug_output) {
            std::cout << "H_elastic computed: " << H_elastic.rows() << "x" << H_elastic.cols() 
                      << ", nnz=" << H_elastic.nonZeros() << std::endl;
        }

        // compute Y = x^n + h*v^n + h^2*M^{-1}*f_ext
        // Since f_ext = m_per * a_ext, we have: h^2 * M^{-1} * f_ext = h^2 * a_ext
        // Use .rowwise() for correct broadcasting of acceleration_ext
        Eigen::MatrixXd Y = X + h * vel;
        Y.rowwise() += (h * h) * acceleration_ext.transpose();

        // Add collision force to external forces
        if (enable_sphere_collision) {
            Eigen::MatrixXd acc_collision = acceleration_collision / mass_per_vertex;
            Y += (h * h) * acc_collision;
            
            if (enable_debug_output) {
                std::cout << "Collision acceleration added to Y:" << std::endl;
                std::cout << "  Range: [" << acc_collision.minCoeff() 
                          << ", " << acc_collision.maxCoeff() << "]" << std::endl;
                std::cout << "  Norm: " << acc_collision.norm() << std::endl;
                std::cout << "  Y range after collision: [" << Y.minCoeff() 
                          << ", " << Y.maxCoeff() << "]" << std::endl;
            }
        }

        if (enable_debug_output) {
            std::cout << "Y computed: range [" << Y.minCoeff() << ", " << Y.maxCoeff() << "]" << std::endl;
            std::cout << "X - Y range: [" << (X - Y).minCoeff() << ", " << (X - Y).maxCoeff() << "]" << std::endl;
        }

        // Compute grad_g = (1/h^2)*M*(x - y) + grad_E(x)
        Eigen::MatrixXd grad_E = computeGrad(stiffness);
        Eigen::MatrixXd grad_inertia = (1.0 / (h * h)) * mass_per_vertex * (X - Y);
        Eigen::MatrixXd grad_g = grad_inertia + grad_E;

        if (enable_debug_output) {
            std::cout << "grad_E (elastic) range: [" << grad_E.minCoeff() 
                      << ", " << grad_E.maxCoeff() << "]" << std::endl;
            std::cout << "grad_E norm: " << grad_E.norm() << std::endl;
            std::cout << "grad_inertia range: [" << grad_inertia.minCoeff() 
                      << ", " << grad_inertia.maxCoeff() << "]" << std::endl;
            std::cout << "grad_inertia norm: " << grad_inertia.norm() << std::endl;
            std::cout << "grad_g (total) range: [" << grad_g.minCoeff() 
                      << ", " << grad_g.maxCoeff() << "]" << std::endl;
            std::cout << "grad_g norm: " << grad_g.norm() << std::endl;
            
            if (enable_sphere_collision) {
                std::cout << "\n=== COLLISION FORCE EFFECT ANALYSIS ===" << std::endl;
                std::cout << "Expected collision acceleration norm: " 
                          << (acceleration_collision / mass_per_vertex).norm() << std::endl;
                std::cout << "Inertia term (contains collision): " << grad_inertia.norm() << std::endl;
                std::cout << "Ratio inertia/elastic: " << grad_inertia.norm() / grad_E.norm() << std::endl;
                std::cout << "=== END COLLISION FORCE EFFECT ANALYSIS ===\n" << std::endl;
            }
        }

        // Apply Dirichlet boundary condition: set grad_g to 0 for fixed points
        int n_fixed = 0;
        for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
            if (dirichlet_bc_mask[i]) {
                grad_g.row(i).setZero();
                n_fixed++;
            }
        }
        
        if (enable_debug_output) {
            std::cout << "Applied Dirichlet BC to " << n_fixed << " fixed vertices" << std::endl;
        }

        // Compute Hessian_g = (1/h^2)*M + H
        Eigen::SparseMatrix<double> H_g = H_elastic;
        
        // Add mass matrix term: (1/h^2)*M
        // Use Row-Major indexing to match Hessian and boundary condition parsing
        std::vector<Trip_d> mass_triplets;
        double mass_term = mass_per_vertex / (h * h);
        for (int i = 0; i < n_vertices; i++) {
            for (int j = 0; j < 3; j++) {
                mass_triplets.push_back(Trip_d(i * 3 + j, i * 3 + j, mass_term));
            }
        }
        Eigen::SparseMatrix<double> M_term(n_vertices * 3, n_vertices * 3);
        M_term.setFromTriplets(mass_triplets.begin(), mass_triplets.end());
        H_g += M_term;

        if (enable_debug_output) {
            std::cout << "Mass term added: mass_term=" << mass_term << std::endl;
            std::cout << "H_g (before BC) nnz: " << H_g.nonZeros() << std::endl;
        }

        // Apply Dirichlet boundary condition: modify Hessian for fixed points
        // Set diagonal to 1 and off-diagonal to 0 for fixed DOFs
        // CRITICAL: Must check BOTH row and col to maintain matrix symmetry!
        std::vector<Trip_d> corrected_triplets;
        int n_fixed_dofs = 0;
        int n_free_dofs = 0;
        int n_mixed_dofs = 0;
        
        for (int k = 0; k < H_g.outerSize(); ++k) {
            for (Eigen::SparseMatrix<double>::InnerIterator it(H_g, k); it; ++it) {
                int row = it.row();
                int col = it.col();
                int row_vertex_idx = row / 3;
                int col_vertex_idx = col / 3;
                
                // Check if BOTH DOFs belong to fixed vertices
                bool is_row_fixed = dirichlet_bc_mask[row_vertex_idx];
                bool is_col_fixed = dirichlet_bc_mask[col_vertex_idx];
                
                if (is_row_fixed && is_col_fixed) {
                    // Both fixed: set diagonal to 1, skip off-diagonal
                    if (row == col) {
                        corrected_triplets.push_back(Trip_d(row, col, 1.0));
                        n_fixed_dofs++;
                    }
                    // Skip off-diagonal elements (effectively setting them to 0)
                }
                else if (!is_row_fixed && !is_col_fixed) {
                    // Both not fixed: keep original value
                    corrected_triplets.push_back(Trip_d(row, col, it.value()));
                    n_free_dofs++;
                }
                else {
                    // One fixed, one not fixed: skip this element
                    // This ensures matrix symmetry by removing asymmetric connections
                    n_mixed_dofs++;
                }
            }
        }
        
        if (enable_debug_output) {
            std::cout << "Hessian BC stats: fixed_dofs=" << n_fixed_dofs 
                      << ", free_dofs=" << n_free_dofs 
                      << ", mixed_dofs=" << n_mixed_dofs << std::endl;
        }
        
        // Rebuild H_g with corrected boundary conditions
        H_g.setFromTriplets(corrected_triplets.begin(), corrected_triplets.end());
        
        // Add regularization to ensure positive definiteness
        // Add epsilon to diagonal to improve conditioning
        // Efficient: add to triplets before building matrix
        double epsilon = 1e-6;
        for (int i = 0; i < n_vertices * 3; i++) {
            corrected_triplets.push_back(Trip_d(i, i, epsilon));
        }
        H_g.setFromTriplets(corrected_triplets.begin(), corrected_triplets.end());
        H_g.makeCompressed();

        if (enable_debug_output) {
            std::cout << "H_g (after BC + reg) nnz: " << H_g.nonZeros() << std::endl;
        }

        // Flatten matrices for solving
        Eigen::VectorXd grad_g_flatten = flatten(grad_g);
        Eigen::VectorXd rhs = -grad_g_flatten;
        
        // Apply Dirichlet boundary condition: set rhs to 0 for fixed DOFs
        // Use Row-Major indexing to match Hessian and boundary condition parsing
        for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
            if (dirichlet_bc_mask[i]) {
                for (int j = 0; j < 3; j++) {
                    rhs[i * 3 + j] = 0.0;
                }
            }
        }

        // Solve Newton's search direction with linear solver
        Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> solver;
        solver.compute(H_g);
        
        if (enable_debug_output) {
            std::cout << "Solver compute: " << (solver.info() == Eigen::Success ? "SUCCESS" : "FAILED") << std::endl;
        }
        
        if (solver.info() != Eigen::Success) {
            std::cerr << "Solver failed to decompose matrix! Matrix may not be positive definite." << std::endl;
            std::cerr << "  Try reducing stiffness or time step h" << std::endl;
        }
        
        Eigen::VectorXd delta_X_flatten = solver.solve(rhs);
        
        if (enable_debug_output) {
            std::cout << "Solver solve: " << (solver.info() == Eigen::Success ? "SUCCESS" : "FAILED") << std::endl;
        }
        
        if (solver.info() != Eigen::Success) {
            std::cerr << "Solver failed to solve system!" << std::endl;
        }
        
        if (enable_debug_output) {
            double delta_norm = delta_X_flatten.norm();
            std::cout << "Delta X norm: " << delta_norm << std::endl;
            std::cout << "Delta X range: [" << delta_X_flatten.minCoeff() << ", " << delta_X_flatten.maxCoeff() << "]" << std::endl;
            if (delta_norm > 1.0) {
                std::cout << "WARNING: Large displacement detected!" << std::endl;
            }
        }

        // Unflatten and update X
        Eigen::MatrixXd delta_X = unflatten(delta_X_flatten);

        if (enable_debug_output) {
            std::cout << "delta_X unflattened: range [" << delta_X.minCoeff() << ", " << delta_X.maxCoeff() << "]" << std::endl;
        }
        
        // Safety check: limit maximum displacement per step
        // This prevents numerical explosion when the system becomes unstable
        double max_delta = 100.0;  // Maximum allowed displacement
        double delta_norm = delta_X.norm();
        bool clamped = false;
        if (delta_norm > max_delta) {
            if (enable_debug_output) {
                std::cout << "CLAMPING displacement from " << delta_norm << " to " << max_delta << std::endl;
            }
            delta_X = delta_X * (max_delta / delta_norm);
            clamped = true;
        }
        
        Eigen::MatrixXd X_old = X;
        X += delta_X;

        if (enable_debug_output) {
            std::cout << "X updated: range [" << X.minCoeff() << ", " << X.maxCoeff() << "]" << std::endl;
            std::cout << "X change: range [" << (X - X_old).minCoeff() << ", " << (X - X_old).maxCoeff() << "]" << std::endl;
        }

        // Update velocity: v = (x^{n+1} - x^n) / h
        vel = delta_X / h;

        if (enable_debug_output) {
            std::cout << "vel updated: range [" << vel.minCoeff() << ", " << vel.maxCoeff() << "]" << std::endl;
            std::cout << "vel norm: " << vel.norm() << std::endl;
        }

        // Apply Dirichlet boundary condition: set velocity to 0 for fixed points
        for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
            if (dirichlet_bc_mask[i]) {
                vel.row(i).setZero();
            }
        }

        if (enable_debug_output) {
            std::cout << "After BC: vel range [" << vel.minCoeff() << ", " << vel.maxCoeff() << "]" << std::endl;
            std::cout << "=== IMPLICIT EULER STEP END ===\n" << std::endl;
        }

        TOC(step)
    }
    else if (time_integrator == SEMI_IMPLICIT_EULER) {
        // Semi-implicit Euler
        Eigen::MatrixXd acceleration =
            -computeGrad(stiffness) / mass_per_vertex;
        acceleration.rowwise() += acceleration_ext.transpose();

        // -----------------------------------------------
        // (HW Optional)
        if (enable_sphere_collision) {
            // CRITICAL: Convert force to acceleration by dividing by mass
            Eigen::MatrixXd acc_collision = acceleration_collision / mass_per_vertex;
            
            if (enable_debug_output) {
                std::cout << "\n=== COLLISION FORCE ANALYSIS ===" << std::endl;
                std::cout << "Collision force (before / mass):" << std::endl;
                std::cout << "  Range: [" << acceleration_collision.minCoeff() 
                          << ", " << acceleration_collision.maxCoeff() << "]" << std::endl;
                std::cout << "  Norm: " << acceleration_collision.norm() << std::endl;
                std::cout << "Collision acceleration (after / mass):" << std::endl;
                std::cout << "  Range: [" << acc_collision.minCoeff() 
                          << ", " << acc_collision.maxCoeff() << "]" << std::endl;
                std::cout << "  Norm: " << acc_collision.norm() << std::endl;
                std::cout << "  Ratio to gravity: " << acc_collision.norm() / 9.8 << std::endl;
                
                // Compare with elastic force
                Eigen::MatrixXd acc_elastic = -computeGrad(stiffness) / mass_per_vertex;
                std::cout << "Elastic acceleration:" << std::endl;
                std::cout << "  Range: [" << acc_elastic.minCoeff() 
                          << ", " << acc_elastic.maxCoeff() << "]" << std::endl;
                std::cout << "  Norm: " << acc_elastic.norm() << std::endl;
                
                // Compare with gravity
                std::cout << "Gravity acceleration:" << std::endl;
                std::cout << "  (0, 0, " << gravity.z() << ")" << std::endl;
                std::cout << "  Norm: " << gravity.norm() << std::endl;
                
                std::cout << "=== END COLLISION FORCE ANALYSIS ===\n" << std::endl;
            }
            
            acceleration += acc_collision;
        }
        // -----------------------------------------------

        // (HW TODO): Implement semi-implicit Euler time integration
        // Apply Dirichlet boundary condition: set acceleration to 0 for fixed points
        for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
            if (dirichlet_bc_mask[i]) {
                acceleration.row(i).setZero();
            }
        }

        // Update velocity: v^{n+1} = v^n + h * a
        Eigen::MatrixXd vel_old = vel;
        vel += h * acceleration;

        if (enable_debug_output && enable_sphere_collision) {
            // Analyze velocity change for colliding vertices
            std::cout << "\n=== VELOCITY CHANGE ANALYSIS ===" << std::endl;
            int n_analyzed = 0;
            for (int i = 0; i < n_vertices && n_analyzed < 5; i++) {
                Eigen::Vector3d x = X.row(i).transpose();
                Eigen::Vector3d diff = x - sphere_center.cast<double>();
                double dist = diff.norm();
                double collision_dist = collision_scale_factor * sphere_radius;
                
                if (dist < collision_dist) {
                    Eigen::Vector3d vel_change = (vel.row(i) - vel_old.row(i)).transpose();
                    std::cout << "Colliding vertex " << i << ":" << std::endl;
                    std::cout << "  Position: (" << x.x() << ", " << x.y() << ", " << x.z() << ")" << std::endl;
                    std::cout << "  Distance to sphere: " << dist << std::endl;
                    std::cout << "  Velocity change: (" << vel_change.x() 
                              << ", " << vel_change.y() << ", " << vel_change.z() << ")" << std::endl;
                    std::cout << "  Velocity change magnitude: " << vel_change.norm() << std::endl;
                    std::cout << "  New velocity: (" << vel.row(i).x() 
                              << ", " << vel.row(i).y() << ", " << vel.row(i).z() << ")" << std::endl;
                    n_analyzed++;
                }
            }
            std::cout << "Total velocity change norm: " << (vel - vel_old).norm() << std::endl;
            std::cout << "=== END VELOCITY CHANGE ANALYSIS ===\n" << std::endl;
        }

        // Apply damping
        if (enable_damping) {
            vel *= damping;
        }

        // Apply Dirichlet boundary condition: set velocity to 0 for fixed points
        for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
            if (dirichlet_bc_mask[i]) {
                vel.row(i).setZero();
            }
        }

        // Update position: x^{n+1} = x^n + h * v^{n+1}
        Eigen::MatrixXd X_old = X;
        X += h * vel;
        
        if (enable_debug_output && enable_sphere_collision) {
            // Analyze position change for colliding vertices
            std::cout << "\n=== POSITION CHANGE ANALYSIS ===" << std::endl;
            int n_analyzed = 0;
            for (int i = 0; i < n_vertices && n_analyzed < 5; i++) {
                Eigen::Vector3d x = X.row(i).transpose();
                Eigen::Vector3d diff = x - sphere_center.cast<double>();
                double dist = diff.norm();
                double collision_dist = collision_scale_factor * sphere_radius;
                
                if (dist < collision_dist) {
                    Eigen::Vector3d pos_change = (X.row(i) - X_old.row(i)).transpose();
                    std::cout << "Colliding vertex " << i << ":" << std::endl;
                    std::cout << "  Position change: (" << pos_change.x() 
                              << ", " << pos_change.y() << ", " << pos_change.z() << ")" << std::endl;
                    std::cout << "  Position change magnitude: " << pos_change.norm() << std::endl;
                    std::cout << "  New position: (" << X.row(i).x() 
                              << ", " << X.row(i).y() << ", " << X.row(i).z() << ")" << std::endl;
                    n_analyzed++;
                }
            }
            std::cout << "Total position change norm: " << (X - X_old).norm() << std::endl;
            std::cout << "=== END POSITION CHANGE ANALYSIS ===\n" << std::endl;
        }
    }
    else {
        std::cerr << "Unknown time integrator!" << std::endl;
        return;
    }
}

// There are different types of mass spring energy:
// For this homework we will adopt Prof. Huamin Wang's energy definition
// introduced in GAMES103 course Lecture 2 E = 0.5 * stiffness * sum_{i=1}^{n}
// (||x_i - x_j|| - l)^2 There exist other types of energy definition, e.g.,
// Prof. Minchen Li's energy definition
// https://www.cs.cmu.edu/~15769-f23/lec/3_Mass_Spring_Systems.pdf
double MassSpring::computeEnergy(double stiffness)
{
    double sum = 0.;
    unsigned i = 0;
    for (const auto& e : E) {
        auto diff = X.row(e.first) - X.row(e.second);
        auto l = E_rest_length[i];
        sum += 0.5 * stiffness * std::pow((diff.norm() - l), 2);
        i++;
    }
    return sum;
}

Eigen::MatrixXd MassSpring::computeGrad(double stiffness) 
{
    Eigen::MatrixXd g = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    unsigned i = 0;
    for (const auto& e : E) {
        auto diff = X.row(e.first) - X.row(e.second);
        auto l = E_rest_length[i];
        double norm = diff.norm();
        
        // Increase stability threshold to prevent numerical issues
        double threshold = 1e-8;
        if (norm > threshold) {
            // Handle extreme deformation: both compression and stretching
            // Limit the gradient to prevent numerical explosion
            double compression_ratio = l / norm;
            double stretch_ratio = norm / l;
            
            if (compression_ratio > 100) {
                // Spring is compressed to less than 1% of rest length
                norm = l / 100.0;  // Cap the norm to prevent explosion
            }
            else if (stretch_ratio > 100) {
                // Spring is stretched to more than 100x rest length
                norm = l * 100.0;  // Cap the norm to prevent explosion
            }
            
            auto grad = stiffness * (norm - l) * (diff / norm);
            g.row(e.first) += grad;
            g.row(e.second) -= grad;
        }
        i++;
    }
    return g;
}

Eigen::SparseMatrix<double> MassSpring::computeHessianSparse(double stiffness)
{
    unsigned n_vertices = X.rows();
    Eigen::SparseMatrix<double> H(n_vertices * 3, n_vertices * 3);

    unsigned i = 0;
    auto k = stiffness;
    const auto I = Eigen::MatrixXd::Identity(3, 3);
    std::vector<Trip_d> triplets;

    for (const auto& e : E) {
        auto diff = X.row(e.first) - X.row(e.second);
        double norm = diff.norm();
        double l = E_rest_length[i];

        if (norm > 1e-10) {
            Eigen::Vector3d x_i = diff.transpose();
            Eigen::Matrix3d H_i;

            // Handle positive definiteness: when L > ||x_i||, use simplified Hessian
            // This ensures the Hessian is positive definite even when springs are compressed
            if (l > norm) {
                // Simplified Hessian for compression: H_i ≈ k * (x_i * x_i^T) / ||x_i||^2
                H_i = k * (x_i * x_i.transpose()) / (norm * norm);
            }
            else {
                // Full Hessian from formula (7):
                // H_i = k * (x_i * x_i^T) / ||x_i||^2 + k * (1 - L/||x_i||) * (I - (x_i * x_i^T) / ||x_i||^2)
                H_i = k * (x_i * x_i.transpose()) / (norm * norm) +
                       k * (1.0 - l / norm) * (I - (x_i * x_i.transpose()) / (norm * norm));
            }

            // Assemble Hessian into the global sparse matrix
            // For vertices e.first and e.second, we need to add/subtract the 3x3 blocks
            int idx1 = e.first;
            int idx2 = e.second;

            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < 3; col++) {
                    double val = H_i(row, col);
                    // CRITICAL: Use standard Row-Major indexing to match boundary condition parsing!
                    // Row-Major produces: [x0,y0,z0,x1,y1,z1,...]^T
                    // Index for vertex i, component j is: 3 * i + j
                    // Diagonal blocks (positive)
                    triplets.push_back(Trip_d(idx1 * 3 + row, idx1 * 3 + col, val));
                    triplets.push_back(Trip_d(idx2 * 3 + row, idx2 * 3 + col, val));
                    // Off-diagonal blocks (negative)
                    triplets.push_back(Trip_d(idx1 * 3 + row, idx2 * 3 + col, -val));
                    triplets.push_back(Trip_d(idx2 * 3 + row, idx1 * 3 + col, -val));
                }
            }
        }

        i++;
    }

    H.setFromTriplets(triplets.begin(), triplets.end());
    H.makeCompressed();
    return H;
}

bool MassSpring::checkSPD(const Eigen::SparseMatrix<double>& A)
{
    // Eigen::SimplicialLDLT<SparseMatrix_d> ldlt(A);
    // return ldlt.info() == Eigen::Success;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(A);
    auto eigen_values = es.eigenvalues();
    return eigen_values.minCoeff() >= 1e-10;
}

void MassSpring::reset()
{
    std::cout << "reset" << std::endl;
    this->X = this->init_X;
    this->vel.setZero();
}

// ----------------------------------------------------------------------------------
// (HW Optional) Bonus part
Eigen::MatrixXd MassSpring::getSphereCollisionForce(
    Eigen::Vector3d center,
    double radius)
{
    if (enable_debug_output) {
        std::cout << "\n=== SPHERE COLLISION DETECTION ===" << std::endl;
        std::cout << "Sphere center: (" << center.x() << ", " << center.y() << ", " << center.z() << ")" << std::endl;
        std::cout << "Sphere radius: " << radius << std::endl;
        std::cout << "Collision distance (s*r): " << collision_scale_factor * radius << std::endl;
        std::cout << "Penalty k: " << collision_penalty_k << std::endl;
    }
    
    Eigen::MatrixXd force = Eigen::MatrixXd::Zero(X.rows(), X.cols());
    int n_colliding = 0;
    double max_penetration = 0.0;
    double min_dist = std::numeric_limits<double>::max();
    
    for (int i = 0; i < X.rows(); i++) {
        Eigen::Vector3d x = X.row(i).transpose();
        Eigen::Vector3d diff = x - center;
        double dist = diff.norm();
        
        min_dist = std::min(min_dist, dist);
        
        // Check if vertex is inside or close to the sphere
        double collision_dist = collision_scale_factor * radius;
        double penetration = collision_dist - dist;
        
        if (penetration > 0 && dist > 1e-10) {
            // Apply penalty force: f = k^penalty * max(s*r - ||x-c||, 0) * (x-c)/||x-c||
            Eigen::Vector3d force_vec = collision_penalty_k * penetration * (diff / dist);
            force.row(i) = force_vec.transpose();
            n_colliding++;
            max_penetration = std::max(max_penetration, penetration);
            
            if (enable_debug_output && n_colliding <= 3) {
                std::cout << "  Vertex " << i << ": dist=" << dist 
                          << ", penetration=" << penetration 
                          << ", force=" << force_vec.norm() << std::endl;
            }
        }
    }
    
    if (enable_debug_output) {
        std::cout << "Min distance to sphere center: " << min_dist << std::endl;
        std::cout << "Colliding vertices: " << n_colliding << " / " << X.rows() << std::endl;
        if (n_colliding > 0) {
            std::cout << "Max penetration: " << max_penetration << std::endl;
            std::cout << "Force range: [" << force.minCoeff() << ", " << force.maxCoeff() << "]" << std::endl;
            std::cout << "Total collision force norm: " << force.norm() << std::endl;
        }
        std::cout << "=== END SPHERE COLLISION ===\n" << std::endl;
    }
    
    return force;
}
// ----------------------------------------------------------------------------------

bool MassSpring::set_dirichlet_bc_mask(const std::vector<bool>& mask)
{
    if (mask.size() == X.rows()) {
        dirichlet_bc_mask = mask;
        return true;
    }
    else
        return false;
}

bool MassSpring::update_dirichlet_bc_vertices(const MatrixXd& control_vertices)
{
    for (int i = 0; i < dirichlet_bc_control_pair.size(); i++) {
        int idx = dirichlet_bc_control_pair[i].first;
        int control_idx = dirichlet_bc_control_pair[i].second;
        X.row(idx) = control_vertices.row(control_idx);
    }

    return true;
}

bool MassSpring::init_dirichlet_bc_vertices_control_pair(
    const MatrixXd& control_vertices,
    const std::vector<bool>& control_mask)
{
    if (control_mask.size() != control_vertices.rows())
        return false;

    // TODO: optimize this part from O(n) to O(1)
    // First, get selected_control_vertices
    std::vector<VectorXd> selected_control_vertices;
    std::vector<int> selected_control_idx;
    for (int i = 0; i < control_mask.size(); i++) {
        if (control_mask[i]) {
            selected_control_vertices.push_back(control_vertices.row(i));
            selected_control_idx.push_back(i);
        }
    }

    // Then update mass spring fixed vertices
    for (int i = 0; i < dirichlet_bc_mask.size(); i++) {
        if (dirichlet_bc_mask[i]) {
            // O(n^2) nearest point search, can be optimized
            // -----------------------------------------
            int nearest_idx = 0;
            double nearst_dist = 1e6;
            VectorXd X_i = X.row(i);
            for (int j = 0; j < selected_control_vertices.size(); j++) {
                double dist = (X_i - selected_control_vertices[j]).norm();
                if (dist < nearst_dist) {
                    nearst_dist = dist;
                    nearest_idx = j;
                }
            }
            //-----------------------------------------

            X.row(i) = selected_control_vertices[nearest_idx];
            dirichlet_bc_control_pair.push_back(
                std::make_pair(i, selected_control_idx[nearest_idx]));
        }
    }

    return true;
}

}  // namespace USTC_CG::mass_spring