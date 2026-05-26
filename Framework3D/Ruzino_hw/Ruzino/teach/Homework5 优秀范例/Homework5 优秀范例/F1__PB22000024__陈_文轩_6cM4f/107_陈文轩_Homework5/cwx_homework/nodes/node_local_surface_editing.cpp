#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include <Eigen/Sparse>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <queue>
#include <unordered_set>

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(local_surface_editing)
{
    b.add_input<Geometry>("Original mesh");
    b.add_input<pxr::VtVec3fArray>("Changed vertices");
    b.add_input<std::vector<size_t>>("Control Points Indices");
    b.add_input<std::list<size_t>>("Picked Vertex Index (Left Click)");
    b.add_output<pxr::VtVec3fArray>("New vertices");
}

NODE_EXECUTION_FUNCTION(local_surface_editing)
try 
{
    auto input = params.get_input<Geometry>("Original mesh");
    pxr::VtVec3fArray changed_vertices = params.get_input<pxr::VtVec3fArray>("Changed vertices");
    std::vector<size_t> control_points = params.get_input<std::vector<size_t>>("Control Points Indices");
    std::list<size_t> picked_vertex_indices = params.get_input<std::list<size_t>>("Picked Vertex Index (Left Click)");

    if (!input.get_component<MeshComponent>()) 
    {
        throw std::runtime_error("Input mesh is invalid");
    }
    if (control_points.empty()) 
    {
        throw std::runtime_error("At least one control point required");
    }

    auto halfedge_mesh = operand_to_openmesh(&input);
    using Mesh = decltype(halfedge_mesh)::element_type;
    auto& mesh = *halfedge_mesh;
    const int n_vertices = mesh.n_vertices();

    std::vector<OpenMesh::Vec3f> original_positions;
    original_positions.reserve(n_vertices);
    for (auto vh : mesh.vertices()) 
    {
        original_positions.push_back(mesh.point(vh));
    }

    std::unordered_set<size_t> boundary(picked_vertex_indices.begin(), picked_vertex_indices.end());
    for (auto vh : mesh.vertices()) 
    {
        if (mesh.is_boundary(vh)) 
        {
            boundary.insert(vh.idx());
        }
    }

    std::unordered_set<size_t> region_set;
    std::queue<size_t> q;
    std::unordered_set<size_t> visited;

    for (auto cp : control_points) 
    {
        if (cp >= n_vertices) continue;
        q.push(cp);
        visited.insert(cp);
        region_set.insert(cp);
    }

    while (!q.empty()) 
    {
        size_t current = q.front();
        q.pop();

        for (auto vv : mesh.vv_range(Mesh::VertexHandle(current))) 
        {
            size_t neighbor = vv.idx();
            if (visited.find(neighbor) == visited.end() && 
                boundary.find(neighbor) == boundary.end()) 
            {
                visited.insert(neighbor);
                region_set.insert(neighbor);
                q.push(neighbor);
            }
        }
    }

    Eigen::SparseMatrix<double> L(n_vertices, n_vertices);
    Eigen::VectorXd bx(n_vertices), by(n_vertices), bz(n_vertices);
    std::vector<Eigen::Triplet<double>> triplets;

    std::unordered_set<size_t> control_set(control_points.begin(), control_points.end());

    for (int i = 0; i < n_vertices; ++i) 
    {
        if (control_set.count(i)) 
        {
            triplets.emplace_back(i, i, 1.0);
            bx[i] = changed_vertices[i][0];
            by[i] = changed_vertices[i][1];
            bz[i] = changed_vertices[i][2];
        } 
        else if (region_set.count(i)) 
        {
            Mesh::VertexHandle vh(i);
            int valence = 0;
            OpenMesh::Vec3f sum(0,0,0);
            
            for (auto vv : mesh.vv_range(vh)) 
            {
                sum += original_positions[vv.idx()];
                ++valence;
            }

            if (valence == 0) 
            {
                triplets.emplace_back(i, i, 1.0);
                bx[i] = original_positions[i][0];
                by[i] = original_positions[i][1];
                bz[i] = original_positions[i][2];
                continue;
            }

            OpenMesh::Vec3f avg = sum / valence;
            OpenMesh::Vec3f delta = original_positions[i] - avg;

            triplets.emplace_back(i, i, 1.0);
            double weight = -1.0 / valence;
            for (auto vv : mesh.vv_range(vh)) 
            {
                triplets.emplace_back(i, vv.idx(), weight);
            }

            bx[i] = delta[0];
            by[i] = delta[1];
            bz[i] = delta[2];
        } 
        else 
        {
            triplets.emplace_back(i, i, 1.0);
            bx[i] = original_positions[i][0];
            by[i] = original_positions[i][1];
            bz[i] = original_positions[i][2];
        }
    }

    L.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success) 
    {
        throw std::runtime_error("Matrix decomposition failed");
    }

    Eigen::VectorXd Xx = solver.solve(bx);
    Eigen::VectorXd Xy = solver.solve(by);
    Eigen::VectorXd Xz = solver.solve(bz);

    pxr::VtVec3fArray new_vertices;
    new_vertices.resize(n_vertices);
    for (int i = 0; i < n_vertices; ++i) 
    {
        new_vertices[i] = pxr::GfVec3f(Xx[i], Xy[i], Xz[i]);
    }

    params.set_output("New vertices", std::move(new_vertices));
    return true;
}
catch (const std::exception& e) 
{
    throw std::runtime_error(std::string("[Local Surface Editing] ") + e.what());
}

NODE_DECLARATION_UI(local_surface_editing);
NODE_DEF_CLOSE_SCOPE