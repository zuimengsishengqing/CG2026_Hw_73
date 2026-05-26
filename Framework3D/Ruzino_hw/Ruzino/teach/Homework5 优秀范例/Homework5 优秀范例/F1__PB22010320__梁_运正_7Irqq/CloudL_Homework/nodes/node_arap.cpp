#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include <cmath>
#include <time.h>
#include <Eigen/Dense>
#include <Eigen/SparseLU>
#include <Eigen/Sparse>
#include "ARAP.h"
#include "ASAP.h"
#include "Hybrid.h"

/*
** @brief HW5_ARAP_Parameterization
**
** This file presents the basic framework of a "node", which processes inputs
** received from the left and outputs specific variables for downstream nodes to
** use.
**
** - In the first function, node_declare, you can set up the node's input and
** output variables.
**
** - The second function, node_exec is the execution part of the node, where we
** need to implement the node's functionality.
**
** - The third function generates the node's registration information, which
** eventually allows placing this node in the GUI interface.
**
** Your task is to fill in the required logic at the specified locations
** within this template, especially in node_exec.
*/

NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(arap)
{
    b.add_input<Geometry>("Input");
    b.add_input<Geometry>("ParamMesh");
    b.add_input<int>("Iterations").min(1).max(30).default_val(1);
    b.add_input<bool>("UseASAP").default_val(false);
    b.add_input<bool>("UseHybrid").default_val(false);  
    b.add_input<float>("Lambda").min(0.0f).max(1.0f).default_val(1.0f);
    b.add_output<pxr::VtArray<pxr::GfVec2f>>("OutputUV");
}

NODE_EXECUTION_FUNCTION(arap)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("ASAP/ARAP: Need Geometry Input.");
    }

    auto param_input = params.get_input<Geometry>("ParamMesh");
    int iterations = params.get_input<int>("Iterations");
    bool use_asap = params.get_input<bool>("UseASAP");
    bool use_hybrid = params.get_input<bool>("UseHybrid");  
    float lambda = params.get_input<float>("Lambda");

    std::shared_ptr<PolyMesh> param_mesh = param_input.get_component<MeshComponent>() ? 
        USTC_CG::operand_to_openmesh(&param_input) : nullptr;
    auto halfedge_mesh = USTC_CG::operand_to_openmesh(&input);
    //auto param = std::make_unique<USTC_CG::ARAP>(halfedge_mesh, param_mesh);

    static std::unique_ptr<USTC_CG::ARAP> solver;
    static std::shared_ptr<PolyMesh> last_mesh;
    static std::shared_ptr<PolyMesh> last_param_mesh;
    
    // 检查是否需要重新预处理
    bool need_recreate = !solver || solver->needs_precompute(halfedge_mesh, param_mesh);
    if (need_recreate) {
        if (use_asap) {
            solver = std::make_unique<USTC_CG::ASAP>(halfedge_mesh, param_mesh);
        }
        else if (use_hybrid) {  // Hybrid 分支
            solver = std::make_unique<USTC_CG::Hybrid>(halfedge_mesh, param_mesh, lambda);
        }
        else {  // 默认 ARAP
            solver = std::make_unique<USTC_CG::ARAP>(halfedge_mesh, param_mesh);
        }
        solver->precompute();
        last_mesh = halfedge_mesh;
        last_param_mesh = param_mesh;
    }
    if (use_asap) {
        dynamic_cast<USTC_CG::ASAP*>(solver.get())->execute_asap();
    }
    else {
        solver->execute(iterations);
    }

    auto uv_result = solver->get_result();
    params.set_output("OutputUV", uv_result);
}

NODE_DECLARATION_UI(arap);
NODE_DEF_CLOSE_SCOPE