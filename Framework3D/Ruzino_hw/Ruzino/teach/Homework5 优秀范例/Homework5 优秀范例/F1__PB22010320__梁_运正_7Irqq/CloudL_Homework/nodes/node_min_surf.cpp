#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "tutte_parameterizer.h"
#include "min_surf_uni.h"
#include "min_surf_cot.h"
#include "min_surf_floater.h"
#include "min_surf_harmo.h"
#include <Eigen/Sparse>

NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(min_surf_unif)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(min_surf_unif)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("MinSurf Uniform: Need Geometry Input.");
    }

    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    auto param = std::make_unique<USTC_CG::MinSurfUni>(halfedge_mesh);

    param->execute();
    auto geometry = param->get_result();
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(min_surf_unif);

NODE_DECLARATION_FUNCTION(min_surf_cot)
{
    b.add_input<Geometry>("Input");
    b.add_input<Geometry>("Original");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(min_surf_cot)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("MinSurf Cotangent: Need Geometry Input.");
    }

    auto original_input = params.get_input<Geometry>("Original");
    std::shared_ptr<PolyMesh> original_mesh = original_input.get_component<MeshComponent>() ? 
        USTC_CG::operand_to_openmesh(&original_input) : nullptr;

    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    auto param = std::make_unique<USTC_CG::MinSurfCot>(halfedge_mesh, original_mesh);

    param->execute();
    auto geometry = param->get_result();
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(min_surf_cot);

NODE_DECLARATION_FUNCTION(min_surf_floater)
{
    b.add_input<Geometry>("Input");
    b.add_input<Geometry>("Original");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(min_surf_floater)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("MinSurf Floater: Need Geometry Input.");
    }

    auto original_input = params.get_input<Geometry>("Original");
    std::shared_ptr<PolyMesh> original_mesh = original_input.get_component<MeshComponent>() ? 
        USTC_CG::operand_to_openmesh(&original_input) : nullptr;

    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    auto param = std::make_unique<USTC_CG::MinSurfFloater>(halfedge_mesh, original_mesh);

    param->execute();
    auto geometry = param->get_result();
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(min_surf_floater);


NODE_DECLARATION_FUNCTION(min_surf_harmo)
{
    b.add_input<Geometry>("Input");
    b.add_input<Geometry>("Original");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(min_surf_harmo)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("MinSurf Harmo: Need Geometry Input.");
    }

    auto original_input = params.get_input<Geometry>("Original");
    std::shared_ptr<PolyMesh> original_mesh = original_input.get_component<MeshComponent>() ? 
        USTC_CG::operand_to_openmesh(&original_input) : nullptr;

    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    auto param = std::make_unique<USTC_CG::MinSurfHarmo>(halfedge_mesh, original_mesh);

    param->execute();
    auto geometry = param->get_result();
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(min_surf_harmo);

NODE_DEF_CLOSE_SCOPE