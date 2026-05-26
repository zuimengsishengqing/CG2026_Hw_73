#include "sph_base.h"
#include <cmath>
#define M_PI 3.14159265358979323846
#include <omp.h>
#include <iostream>
#include "colormap_jet.h"

namespace USTC_CG::sph_fluid {
using namespace Eigen;
using Real = double;

SPHBase::SPHBase(const Eigen::MatrixXd& X, const Vector3d& box_min, const Vector3d& box_max)
    : init_X_(X),
      X_(X),
      vel_(MatrixXd::Zero(X.rows(), X.cols())),
      box_max_(box_max),
      box_min_(box_min),
      ps_(X, box_min, box_max)
{
}

// ----------------- SPH kernal function and its spatial derivatives, no need to modify -----------------
double SPHBase::W(const Eigen::Vector3d& r, double h)
{
    double h3 = h * h * h;
    double m_k = 8.0 / (M_PI * h3);
    double m_l = 48.0 / (M_PI * h3); 
    const double q = r.norm() / h;
    double result = 0.;

    if (q <= 1.0) {
        if (q <= 0.5) {
            const Real q2 = q * q;
            const Real q3 = q2 * q;
            result = m_k * (6.0 * q3 - 6.0 * q2 + 1.0);
        }
        else {
            result = m_k * (2.0 * pow(1.0 - q, 3.0));
        }
    }
    return result;
}

double SPHBase::W_zero(double h)
{
    double h3 = h * h * h;
    double m_k = 8.0 / (M_PI * h3);
    return m_k;
}

Vector3d SPHBase::grad_W(const Vector3d& r, double h)
{
    double h3 = h * h * h;
    double m_k = 8.0 / (M_PI * h3);
    double m_l = 48.0 / (M_PI * h3);

    const double rl = r.norm();
    const double q = rl / h;
    Vector3d result = Vector3d::Zero();

    if (q <= 1.0 && rl > 1e-9) {
        Vector3d grad_q = r / rl;
        if (q <= 0.5) {
            result = m_l * q * (3.0 * q - 2.0) * grad_q;
        }
        else {
            const Real factor = 1.0 - q;
            result = -m_l * factor * factor * grad_q;
        }
    }
    return result;
}
// ---------------------------------------------------------------------------------------


void SPHBase::compute_density()
{
    // Traverse all particles to compute each particle's density
    // Using rho_i = sum_{j including i} m_j * W(x_i - x_j, h)
    for (auto& p : ps_.particles()) {
        double rho = 0.0;

        // self contribution
        rho += ps_.mass() * W_zero(ps_.h());

        // Then traverse all neighbor fluid particles of p
        for (auto& q : p->neighbors()) {
            rho += ps_.mass() * W(p->x() - q->x(), ps_.h());
        }

        // store density
        p->density() = rho;
    }
}

void SPHBase::compute_pressure()
{
    // Not implemented, should be implemented in children classes WCSPH, IISPH, etc. 
}

void SPHBase::compute_non_pressure_acceleration()
{
    // Traverse all particles to compute each particle's non-pressure acceleration
    // non-pressure acceleration includes gravity and viscosity
    const int dim = 3; // simulation dimension
    for (auto& p : ps_.particles()) {
        if (p->is_boundary()) {
            // for boundary particles we keep zero acceleration for now
            p->acceleration() = Vector3d::Zero();
            continue;
        }

        // start with external force (gravity)
        Vector3d acc = gravity_;

        // add viscosity contributions from neighbors
        for (auto& q : p->neighbors()) {
            acc += compute_viscosity_acceleration(p, q);
        }

        p->acceleration() = acc;
    }
}

// compute viscosity acceleration between two particles
Vector3d SPHBase::compute_viscosity_acceleration(
    const std::shared_ptr<Particle>& p,
    const std::shared_ptr<Particle>& q)
{
    auto v_ij = p->vel() - q->vel();
    auto x_ij = p->x() - q->x();
    Vector3d grad = grad_W(p->x() - q->x(), ps_.h());
    // Use the SPH discretization for Laplacian of velocity:
    // \nabla^2 v_i = 2(d+2) \sum_j (m_j / rho_j) * (v_ij \cdot x_ij) / (|x_ij|^2 + 0.01 h^2) * \nabla W_{ij}
    const double eps = 1e-12;
    const double h = ps_.h();
    const double denom = x_ij.squaredNorm() + 0.01 * h * h;
    double rho_j = q->density();
    if (rho_j <= eps) rho_j = ps_.density0();

    const int d = 3;
    const double factor = 2.0 * (d + 2.0);

    double dot = v_ij.dot(x_ij);
    double coef = factor * (ps_.mass() / rho_j) * (dot / (denom + eps));

    Vector3d laplace_contrib = coef * grad;
    // viscosity_ here is interpreted as kinematic viscosity (nu)
    return viscosity_ * laplace_contrib;
}

// Traverse all particles and compute pressure gradient acceleration
void SPHBase::compute_pressure_gradient_acceleration()
{
    const double eps = 1e-12;
    for (auto& p : ps_.particles()) {
        if (p->is_boundary()) {
            continue;
        }

        Vector3d a_pressure = Vector3d::Zero();

        double rho_i = p->density();
        if (rho_i <= eps) rho_i = ps_.density0();

        double p_i = p->pressure();

        for (auto& q : p->neighbors()) {
            double rho_j = q->density();
            if (rho_j <= eps) rho_j = ps_.density0();

            Vector3d grad = grad_W(p->x() - q->x(), ps_.h());
            double term = ps_.mass() * (p_i / (rho_i * rho_i) + q->pressure() / (rho_j * rho_j));
            a_pressure += term * grad;
        }

        // acceleration contribution from pressure (note the negative sign)
        p->acceleration() += -a_pressure;
    }
}

void SPHBase::step()
{
    // Not implemented, should be implemented in children classes WCSPH, IISPH, etc. 
}


void SPHBase::advect()
{
    for (auto& p : ps_.particles())  
    {

        // ---------------------------------------------------------
        // (HW TODO) Implement the advection step of each particle
        // Remember to check collision after advection

        // Your code here 

        // ---------------------------------------------------------
        // semi-implicit Euler: v_{t+dt} = v_t + dt * a, x_{t+dt} = x_t + dt * v_{t+dt}
        p->vel() += dt_ * p->acceleration();
        p->x() += dt_ * p->vel();

        // collision handling (clamp to simulation box)
        check_collision(p);

        vel_.row(p->idx()) = p->vel().transpose();
        X_.row(p->idx()) = p->x().transpose();
    }
}

// ------------------------------- helper functions -----------------------
// Basic collision detection and process
void SPHBase::check_collision(const std::shared_ptr<Particle>& p)
{
    // coefficient of restitution, you can make this parameter adjustable in the UI 
    double restitution = 0.2; 

    // add epsilon offset to avoid particles sticking to the boundary
    Vector3d eps_ = 0.0001 * (box_max_ - box_min_);

    for (int i = 0; i < 3; i++) {
        if (p->x()[i] < box_min_[i]) {
            p->x()[i] = box_min_[i] + eps_[i];
            p->vel()[i] = -restitution * p->vel()[i];
        }
        if (p->x()[i] > box_max_[i]) {
            p->x()[i] = box_max_[i] - eps_[i];
            p->vel()[i] = -restitution * p->vel()[i];
        }
    }
}

// For display
MatrixXd SPHBase::get_vel_color_jet()
{
    MatrixXd vel_color = MatrixXd::Zero(vel_.rows(), 3);
    double max_vel_norm = vel_.rowwise().norm().maxCoeff();
    double min_vel_norm = vel_.rowwise().norm().minCoeff();

    auto c = colormap_jet;

    for (int i = 0; i < vel_.rows(); i++) {
        double vel_norm = vel_.row(i).norm();
        int idx = 0;
        if (fabs(max_vel_norm - min_vel_norm) > 1e-6) {
            idx = static_cast<int>(
                floor((vel_norm - min_vel_norm) / (max_vel_norm - min_vel_norm) * 255));
        }
        vel_color.row(i) << c[idx][0], c[idx][1], c[idx][2];
    }
    return vel_color;
}

void SPHBase::reset()
{
    X_ = init_X_;
    vel_ = MatrixXd::Zero(X_.rows(), X_.cols());

    for (auto& p : ps_.particles()) {
        p->vel() = Vector3d::Zero();
        p->x() = init_X_.row(p->idx()).transpose();
    }
}

// ---------------------------------------------------------------------------------------
}  // namespace USTC_CG::sph_fluid