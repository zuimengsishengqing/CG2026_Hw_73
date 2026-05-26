#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

#include <Eigen/Sparse>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <limits>

#include "GCore/Components/MeshComponent.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "mass_spring/FastMassSpring.h"
#include "mass_spring/MassSpring.h"
#include "mass_spring/utils.h"

struct MassSpringStorage {
    constexpr static bool has_storage = false;
    std::shared_ptr<USTC_CG::mass_spring::MassSpring> mass_spring;
};

NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(hw9_mass_spring)
{
    b.add_input<Geometry>("Mesh");

    // Simulation parameters
    b.add_input<float>("stiffness").default_val(1000).min(100).max(10000);
    b.add_input<float>("h").default_val(0.0333333333f).min(0.0).max(0.5);
    b.add_input<float>("damping").default_val(0.995).min(0.0).max(1.0);
    b.add_input<float>("gravity").default_val(-9.8).min(-20.).max(20.);

    // --------- HW Optional: if you implement sphere collision, please
    // uncomment the following lines ------------
    b.add_input<float>("collision penalty_k")
        .default_val(10000)
        .min(100)
        .max(100000);
    b.add_input<float>("collision scale factor")
        .default_val(1.1)
        .min(1.0)
        .max(2.0);
    b.add_input<float>("sphere radius").default_val(0.4).min(0.0).max(5.0);
    ;
    b.add_input<float>("sphere center x").default_val(0.0);
    b.add_input<float>("sphere center y").default_val(0.0);
    b.add_input<float>("sphere center z").default_val(0.0);
    // -----------------------------------------------------------------------------------------------------------

    // Useful switches (0 or 1). You can add more if you like.
    b.add_input<int>("time integrator type")
        .default_val(0)
        .min(0)
        .max(1);  // 0 for implicit Euler, 1 for semi-implicit Euler
    b.add_input<int>("enable time profiling").default_val(0).min(0).max(1);
    b.add_input<int>("enable damping").default_val(0).min(0).max(1);
    b.add_input<int>("enable debug output").default_val(0).min(0).max(1);

    // Optional switches
    b.add_input<int>("enable Liu13").default_val(0).min(0).max(1);
    b.add_input<int>("enable sphere collision").default_val(0).min(0).max(1);
    // Volumetric mass-spring toggle (0 = surface cloth, 1 = volumetric grid)
    b.add_input<int>("enable volumetric").default_val(0).min(0).max(1);
    // OpenMP parallelization toggle (0 = serial, 1 = parallel with OpenMP)
    b.add_input<int>("enable parallel").default_val(0).min(0).max(1);
    // PINN integration toggle and model path
    b.add_input<int>("enable PINN").default_val(0).min(0).max(1);
    b.add_input<std::string>("PINN model path").default_val(std::string("source/Plugins/PINN/pinn_burgers_model-11087.pt"));
    
    // FastMassSpring specific parameters
    b.add_input<int>("max iterations").default_val(10).min(1).max(100);

    // Output
    b.add_output<Geometry>("Output Mesh");
}

NODE_EXECUTION_FUNCTION(hw9_mass_spring)
{
    using namespace Eigen;
    using namespace USTC_CG::mass_spring;

    auto& global_payload = params.get_global_payload<GeomPayload&>();
    auto current_time = global_payload.current_time;

    auto& storage = params.get_storage<MassSpringStorage&>();
    auto& mass_spring = storage.mass_spring;

    auto geometry = params.get_input<Geometry>("Mesh");
    auto mesh = geometry.get_component<MeshComponent>();
    if (mesh->get_face_vertex_counts().size() == 0) {
        throw std::runtime_error("Read USD error.");
    }

    std::cout << "Mass Spring: current time = " << current_time << std::endl;
    if (current_time == 0 ||
        !mass_spring) {  // Reset and initialize the mass spring class
        if (mesh) {
            if (mass_spring != nullptr)
                mass_spring.reset();

            auto edges = get_edges(usd_faces_to_eigen(
                mesh->get_face_vertex_counts(),
                mesh->get_face_vertex_indices()));
            auto vertices = usd_vertices_to_eigen(mesh->get_vertices());
            const float k = params.get_input<float>("stiffness");
            const float h = params.get_input<float>("h");

            bool enable_liu13 =
                params.get_input<int>("enable Liu13") == 1 ? true : false;
            bool enable_volumetric = params.get_input<int>("enable volumetric") == 1 ? true : false;
            if (enable_liu13) {
                // HW Optional
                mass_spring =
                    std::make_shared<FastMassSpring>(vertices, edges, k, h, enable_volumetric);
            }
            else
                mass_spring = std::make_shared<MassSpring>(vertices, edges);

            // simulation parameters
            mass_spring->stiffness = k;
            mass_spring->h = params.get_input<float>("h");
            mass_spring->gravity = { 0, 0, params.get_input<float>("gravity") };
            mass_spring->damping = params.get_input<float>("damping");

            // Optional parameters
            // --------- HW Optional: if you implement sphere collision, please
            // uncomment the following lines ------------
            mass_spring->collision_penalty_k =
                params.get_input<float>("collision penalty_k");
            mass_spring->collision_scale_factor =
                params.get_input<float>("collision scale factor");
            float c[3];
            c[0] = params.get_input<float>("sphere center x");
            c[1] = params.get_input<float>("sphere center y");
            c[2] = params.get_input<float>("sphere center z");
            mass_spring->sphere_center = { c[0], c[1], c[2] };
            mass_spring->sphere_radius =
                params.get_input<float>("sphere radius");
            // --------------------------------------------------------------------------------------------------------

            mass_spring->enable_sphere_collision =
                params.get_input<int>("enable sphere collision") == 1 ? true
                                                                      : false;
            mass_spring->enable_damping =
                params.get_input<int>("enable damping") == 1 ? true : false;
            mass_spring->time_integrator =
                params.get_input<int>("time integrator type") == 0
                    ? MassSpring::IMPLICIT_EULER
                    : MassSpring::SEMI_IMPLICIT_EULER;
            mass_spring->enable_time_profiling =
                params.get_input<int>("enable time profiling") == 1 ? true
                                                                    : false;
            mass_spring->enable_debug_output =
                params.get_input<int>("enable debug output") == 1 ? true
                                                                  : false;
            
            // Set max_iter for FastMassSpring
            auto fast_mass_spring = std::dynamic_pointer_cast<FastMassSpring>(mass_spring);
            if (fast_mass_spring) {
                fast_mass_spring->max_iter = params.get_input<int>("max iterations");
                fast_mass_spring->enable_omp_parallel = 
                    params.get_input<int>("enable parallel") == 1 ? true : false;
            }
        }
        else {
            mass_spring = nullptr;
            throw std::runtime_error("Mass Spring: Need Geometry Input.");
        }
    }
    else if (mass_spring)  // otherwise, step forward the simulation
    {
        // If PINN mode is enabled, call external Python inference to replace
        // mesh positions. Otherwise, run regular simulation step.
        bool enable_pinn = params.get_input<int>("enable PINN") == 1 ? true : false;
        if (enable_pinn) {
            std::string model_path = params.get_input<std::string>("PINN model path");
            auto verts = mass_spring->getX();
            int nverts = (int)verts.rows();
            std::string in_csv = "source/Plugins/PINN/pinn_input_tmp.csv";
            std::string out_csv = "source/Plugins/PINN/pinn_output_tmp.csv";

            // Write input CSV: x,y,z,current_time
            std::ofstream ofs(in_csv);
            if (ofs) {
                for (int i = 0; i < nverts; ++i) {
                    ofs << verts(i, 0) << "," << verts(i, 1) << "," << verts(i, 2) << "," << current_time << "\n";
                }
                ofs.close();

                // Build and run python inference command
                std::string cmd = "python \"source/Plugins/PINN/pinn_infer.py\" \"" + model_path + "\" \"" + in_csv + "\" \"" + out_csv + "\"";
                int ret = std::system(cmd.c_str());
                if (ret == 0) {
                    // Read output CSV (expecting three columns per vertex)
                    std::ifstream ifs(out_csv);
                    if (ifs) {
                        Eigen::MatrixXd pred(nverts, 3);
                        for (int i = 0; i < nverts; ++i) {
                            double a = 0, b = 0, c = 0;
                            char comma;
                            ifs >> a >> comma >> b >> comma >> c;
                            // consume remainder of line
                            ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                            pred(i, 0) = a;
                            pred(i, 1) = b;
                            pred(i, 2) = c;
                        }
                        // Update internal state and mesh
                        mass_spring->setX(pred);
                    } else {
                        std::cerr << "PINN: failed to read output CSV: " << out_csv << std::endl;
                    }
                } else {
                    std::cerr << "PINN inference command failed with return code " << ret << std::endl;
                }
            } else {
                std::cerr << "PINN: failed to write input CSV: " << in_csv << std::endl;
            }
        } else {
            mass_spring->step();
        }
    }
    if (mass_spring) {
        mesh->set_vertices(eigen_to_usd_vertices(mass_spring->getX()));
    }
    params.set_output("Output Mesh", geometry);
    return true;
}

NODE_DECLARATION_UI(hw9_mass_spring);
NODE_DEF_CLOSE_SCOPE