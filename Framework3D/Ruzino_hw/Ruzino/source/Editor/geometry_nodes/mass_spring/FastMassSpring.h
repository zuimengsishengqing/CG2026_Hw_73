#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include "MassSpring.h"

namespace USTC_CG::mass_spring {
// Implement the Liu13 paper:
// https://tiantianliu.cn/papers/liu13fast/liu13fast.pdf
class FastMassSpring : public MassSpring {
   public:
    FastMassSpring() = default;
    ~FastMassSpring() = default;

    FastMassSpring(
        const Eigen::MatrixXd& X,
        const EdgeSet& E,
        const float stiffness,
        const float h,
        const bool volumetric = false);
    void step() override;

    unsigned max_iter = 10;  // Number of local/global iterations per time step
    bool enable_omp_parallel = false;  // Enable OpenMP parallelization

   protected:
    void buildSystemMatrix();

    Eigen::SparseMatrix<double> A;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<double>> A_solver;
    bool A_prefactored = false;
    double prefactor_stiffness = 0.0;
    double prefactor_h = 0.0;
    unsigned n_vertices = 0;
    // When true, the object treats the input vertices as a volumetric grid and
    // builds internal springs accordingly (assumes structured cubic grid when possible)
    bool volumetric = false;
    int grid_nx = 0, grid_ny = 0, grid_nz = 0;

    // Quantitative analysis
    double current_time = 0.0;
    double print_interval = 0.5;
    double next_print_time = 0.5;
    unsigned step_count = 0;

   protected:
    void buildVolumetricSpringsIfNeeded();
    double computeTotalEnergy();
    double computeMaxVelocity();
    double computeMaxDeformation();
    double computeMaxStrain();
};
}  // namespace USTC_CG::mass_spring