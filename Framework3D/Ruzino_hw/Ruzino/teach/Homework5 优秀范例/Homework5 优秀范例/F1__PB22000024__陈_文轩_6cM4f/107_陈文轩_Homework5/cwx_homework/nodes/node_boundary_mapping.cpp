#include "GCore/Components/MeshOperand.h"
#include "geom_node_base.h"
#include "GCore/util_openmesh_bind.h"
#include <Eigen/Sparse>

NODE_DEF_OPEN_SCOPE


NODE_DECLARATION_FUNCTION(circle_boundary_mapping)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(circle_boundary_mapping)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>())
    {
        throw std::runtime_error("Boundary Mapping: Need Geometry Input.");
    }

    auto halfedge_mesh = operand_to_openmesh(&input);
    using Mesh = decltype(halfedge_mesh)::element_type; 
    auto& mesh = *halfedge_mesh;

    OpenMesh::SmartHalfedgeHandle start_he;
    for (auto he : mesh.halfedges())
    {
        if (mesh.is_boundary(he))
        {
            start_he = he;
            break;
        }
    }

    if (!start_he.is_valid())
    {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    std::vector<Mesh::VertexHandle> orderedBoundaryVerts;
    {
        auto current_he = start_he;
        do
        {
            orderedBoundaryVerts.push_back(mesh.from_vertex_handle(current_he));
            current_he = mesh.next_halfedge_handle(current_he);
        } 
        while (current_he != start_he);
    }

    int nB = static_cast<int>(orderedBoundaryVerts.size());
    for (int i = 0; i < nB; ++i)
    {
        double theta = 2.0 * M_PI * double(i) / double(nB);
        auto vh = orderedBoundaryVerts[i];
        auto& p = mesh.point(vh);
        p[0] = static_cast<float>(std::cos(theta)) * 0.5 + 0.5;
        p[1] = static_cast<float>(std::sin(theta)) * 0.5 + 0.5;
        p[2] = 0.0f;
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_FUNCTION(square_boundary_mapping)
{
    b.add_input<Geometry>("Input");

    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(square_boundary_mapping)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) 
    {
        throw std::runtime_error("Input does not contain a mesh");
    }
    auto halfedge_mesh = operand_to_openmesh(&input);
    using Mesh = decltype(halfedge_mesh)::element_type;
    auto& mesh = *halfedge_mesh;

    OpenMesh::SmartHalfedgeHandle start_he;
    for (auto he : mesh.halfedges()) 
    {
        if (mesh.is_boundary(he)) 
        {
            start_he = he;
            break;
        }
    }

    if (!start_he.is_valid()) 
    {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    std::vector<typename Mesh::VertexHandle> orderedBoundaryVerts;
    OpenMesh::SmartHalfedgeHandle current_he = start_he;
    do 
    {
        orderedBoundaryVerts.push_back(mesh.from_vertex_handle(current_he));
        current_he = mesh.next_halfedge_handle(current_he);
    } while (current_he != start_he);

    int nB = (int)orderedBoundaryVerts.size();
    for (int i = 0; i < nB; ++i) 
    {
        double t = double(i) / double(nB); 
        double seg = t * 4.0;

        auto& p = mesh.point(orderedBoundaryVerts[i]);
        
        int segment = (int)std::floor(seg);
        double frac = seg - segment;
        
        switch (segment) 
        {
            case 0:
                p[0] = (float)frac;
                p[1] = 1.0f;
                break;
            case 1:
                p[0] = 1.0f;
                p[1] = 1.0f - (float)frac;
                break;
            case 2:
                p[0] = 1.0f - (float)frac;
                p[1] = 0.0f;
                break;
            default:
                p[0] = 0.0f;
                p[1] = (float)frac;
                break;
        }
        
        p[2] = 0.0f;
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}


NODE_DECLARATION_UI(boundary_mapping);
NODE_DEF_CLOSE_SCOPE
