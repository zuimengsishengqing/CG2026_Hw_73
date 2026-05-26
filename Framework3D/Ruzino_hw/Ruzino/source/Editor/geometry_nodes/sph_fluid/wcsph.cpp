#include "wcsph.h"
#include <iostream>
#include <cmath>
using namespace Eigen;

namespace USTC_CG::sph_fluid {

WCSPH::WCSPH(const MatrixXd& X, const Vector3d& box_min, const Vector3d& box_max)
    : SPHBase(X, box_min, box_max)
{
}

void WCSPH::compute_density()
{
    // Compute density using standard SPH summation and compute pressure via EOS
    SPHBase::compute_density();

    // Compute pressure using Equation of State: p = stiffness * ((rho / rho0)^exponent - 1)
    for (auto& p : ps_.particles()) {
        double rho = p->density();
        double rho0 = ps_.density0();
        double val = stiffness_ * (pow(rho / rho0, exponent_) - 1.0);
        if (val < 0.0)
            val = 0.0;
        p->pressure() = val;
    }
}

void WCSPH::step()
{
    TIC(step)
    // -------------------------------------------------------------
    // (HW TODO) Follow the instruction in documents and PPT, 
    // implement the pipeline of fluid simulation 
    // -------------------------------------------------------------

	// Search neighbors, compute density, advect, solve pressure acceleration, etc. 



    // 1. Spatial hashing / neighbor search
    ps_.assign_particles_to_cells();
    ps_.search_neighbors();

    // 2. Compute density (and pressure via EOS)
    compute_density();

    // 3. Compute non-pressure accelerations (viscosity, gravity, ...)
    compute_non_pressure_acceleration();

    // 4. Compute pressure gradient acceleration
    compute_pressure_gradient_acceleration();

    // 5. Integrate velocity and positions
    advect();

    TOC(step)
}
}  // namespace USTC_CG::sph_fluid