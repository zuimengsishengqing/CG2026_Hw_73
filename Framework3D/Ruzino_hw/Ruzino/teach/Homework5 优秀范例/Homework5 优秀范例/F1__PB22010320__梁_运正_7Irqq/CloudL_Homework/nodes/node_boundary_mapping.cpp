#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "tutte_parameterizer.h"
#include "circle_mapping.h"
#include "square_mapping.h"

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(boundary_mapping_circle)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(boundary_mapping_circle)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Boundary Mapping Circle: Need Geometry Input.");
    }

    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    auto mapper = std::make_unique<USTC_CG::CircleMapping>(halfedge_mesh);

    mapper->execute();
    auto geometry = mapper->get_result();
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(boundary_mapping_circle);

NODE_DECLARATION_FUNCTION(boundary_mapping_square)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(boundary_mapping_square)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Boundary Mapping Square: Need Geometry Input.");
    }

    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    auto mapper = std::make_unique<USTC_CG::SquareMapping>(halfedge_mesh);

    mapper->execute();
    auto geometry = mapper->get_result();
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(boundary_mapping_square);

NODE_DEF_CLOSE_SCOPE