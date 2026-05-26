#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include <cmath>
#include <time.h>
#include <Eigen/Sparse>


NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(min_surf)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(min_surf)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) 
    {
        throw std::runtime_error("Minimal Surface: Need Geometry Input.");
        return false;
    }

    auto halfedge_mesh = operand_to_openmesh(&input);

    using Mesh = decltype(halfedge_mesh)::element_type; 
    auto& mesh = *halfedge_mesh;

    std::vector<bool> is_boundary(mesh.n_vertices(), false);
    for (auto vh : mesh.vertices()) 
    {
        if (mesh.is_boundary(vh)) 
        {
            is_boundary[vh.idx()] = true;
        }
    }

    using SpMat = Eigen::SparseMatrix<double>;
    using Triplet = Eigen::Triplet<double>;
    std::vector<Triplet> tripletList;
    tripletList.reserve(mesh.n_vertices() * 7);

    Eigen::VectorXd bx = Eigen::VectorXd::Zero(mesh.n_vertices());
    Eigen::VectorXd by = Eigen::VectorXd::Zero(mesh.n_vertices());
    Eigen::VectorXd bz = Eigen::VectorXd::Zero(mesh.n_vertices());

    for (auto vh : mesh.vertices()) 
    {
        int i = vh.idx();
        if (is_boundary[i]) 
        {
            tripletList.push_back(Triplet(i, i, 1.0));
            auto point = mesh.point(vh);
            bx(i) = point[0];
            by(i) = point[1];
            bz(i) = point[2];
        } 
        else 
        {
            int valence = 0;
            for (auto vv : mesh.vv_range(vh)) 
            {
                valence++;
            }
            tripletList.push_back(Triplet(i, i, (double)valence));
            for (auto vv : mesh.vv_range(vh)) 
            {
                tripletList.push_back(Triplet(i, vv.idx(), -1.0));
            }
        }
    }

    SpMat L(mesh.n_vertices(), mesh.n_vertices());
    L.setFromTriplets(tripletList.begin(), tripletList.end());

    Eigen::SparseLU<SpMat> solver;
    solver.compute(L);

    if (solver.info() != Eigen::Success) 
    {
        throw std::runtime_error("Decomposition failed in Minimal Surface Node");
        return false;
    }

    auto Xx = solver.solve(bx);
    auto Xy = solver.solve(by);
    auto Xz = solver.solve(bz);

    for (auto vh : mesh.vertices()) 
    {
        int i = vh.idx();
        auto& p = mesh.point(vh);
        p[0] = (float)Xx(i);
        p[1] = (float)Xy(i);
        p[2] = (float)Xz(i);
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(min_surf);
NODE_DEF_CLOSE_SCOPE
