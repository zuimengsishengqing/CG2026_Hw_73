#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include <Eigen/Sparse>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <unordered_map>

NODE_DEF_OPEN_SCOPE

struct SolverStorage 
{
    Eigen::SparseMatrix<double> L;          
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver; 
    std::vector<size_t> control_points;
    bool is_cotangent = false; 
};

static std::unordered_map<std::string, SolverStorage> g_solver_cache;


NODE_DECLARATION_FUNCTION(mesh_editing_uniform)
{
    b.add_input<Geometry>("Original mesh");
    b.add_input<pxr::VtVec3fArray>("Changed vertices");
    b.add_input<std::vector<size_t>>("Control Points Indices");
    b.add_output<pxr::VtVec3fArray>("New vertices");
}


NODE_EXECUTION_FUNCTION(mesh_editing_uniform)
try 
{
	auto input = params.get_input<Geometry>("Original mesh");
	pxr::VtVec3fArray changed_vertices = params.get_input<pxr::VtVec3fArray>("Changed vertices");
	std::vector<size_t> control_points = params.get_input<std::vector<size_t>>("Control Points Indices");

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

	std::string cache_key = "uniform_" + std::to_string(n_vertices) + "_" + std::to_string(control_points.size());

	auto& storage = g_solver_cache[cache_key];
	
	bool need_rebuild = (storage.control_points != control_points) || (storage.L.rows() != n_vertices);

	if (need_rebuild) 
	{
		std::vector<Eigen::Triplet<double>> triplets;
		triplets.reserve(n_vertices * 6);

		std::vector<bool> is_control(n_vertices, false);
		for (auto idx : control_points) 
		{
			if (idx < n_vertices) is_control[idx] = true;
		}

		for (auto vh : mesh.vertices()) 
		{
			const int i = vh.idx();
			
			if (is_control[i]) 
			{
				triplets.emplace_back(i, i, 1.0);
				continue;
			}

			int valence = 0;
			for (auto vv : mesh.vv_range(vh)) ++valence;
			if (valence == 0) continue;

			triplets.emplace_back(i, i, 1.0);

			const double weight = -1.0 / valence;
			for (auto vv : mesh.vv_range(vh)) 
			{
				triplets.emplace_back(i, vv.idx(), weight);
			}
		}

		storage.L.resize(n_vertices, n_vertices);
		storage.L.setFromTriplets(triplets.begin(), triplets.end());
		storage.solver.compute(storage.L);
		storage.control_points = control_points;
	}

	Eigen::VectorXd bx(n_vertices), by(n_vertices), bz(n_vertices);
	for (auto vh : mesh.vertices()) 
	{
		const int i = vh.idx();
		if (std::find(control_points.begin(), control_points.end(), i) != control_points.end()) 
		{
			bx[i] = changed_vertices[i][0];
			by[i] = changed_vertices[i][1];
			bz[i] = changed_vertices[i][2];
		} 
		else 
		{
			OpenMesh::Vec3f sum(0,0,0);
			int valence = 0;
			for (auto vv : mesh.vv_range(vh)) 
			{
				sum += mesh.point(vv);
				++valence;
			}
			if (valence > 0) 
			{
				const auto delta = mesh.point(vh) - sum / valence;
				bx[i] = delta[0];
				by[i] = delta[1];
				bz[i] = delta[2];
			} 
			else 
			{
				bx[i] = mesh.point(vh)[0];
				by[i] = mesh.point(vh)[1];
				bz[i] = mesh.point(vh)[2];
			}
		}
	}

	const Eigen::VectorXd Xx = storage.solver.solve(bx);
	const Eigen::VectorXd Xy = storage.solver.solve(by);
	const Eigen::VectorXd Xz = storage.solver.solve(bz);

	pxr::VtVec3fArray new_vertices;
	new_vertices.resize(n_vertices);
	for (int i = 0; i < n_vertices; ++i) 
	{
		new_vertices[i] = pxr::GfVec3f(Xx[i], Xy[i], Xz[i]);
	}

	params.set_output("New vertices", std::move(new_vertices));
	return true;
}
catch (const std::exception& e) {
	throw std::runtime_error(std::string("[Uniform] ") + e.what());
}


NODE_DECLARATION_FUNCTION(mesh_editing_cotangent)
{
    b.add_input<Geometry>("Original mesh");
    b.add_input<pxr::VtVec3fArray>("Changed vertices");
    b.add_input<std::vector<size_t>>("Control Points Indices");
    b.add_output<pxr::VtVec3fArray>("New vertices");
}

NODE_EXECUTION_FUNCTION(mesh_editing_cotangent)
try 
{
    auto input = params.get_input<Geometry>("Original mesh");
    pxr::VtVec3fArray changed_vertices = params.get_input<pxr::VtVec3fArray>("Changed vertices");
    std::vector<size_t> control_points = params.get_input<std::vector<size_t>>("Control Points Indices");

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

    std::string cache_key = "cotangent_" + std::to_string(n_vertices) + "_" + std::to_string(control_points.size());

    auto& storage = g_solver_cache[cache_key];
    storage.is_cotangent = true;

    bool need_rebuild = (storage.control_points != control_points) || (storage.L.rows() != n_vertices);

	
    if (need_rebuild) 
	{
        std::vector<Eigen::Triplet<double>> triplets;
        triplets.reserve(n_vertices * 6);

        std::vector<bool> is_control(n_vertices, false);
        for (auto idx : control_points) 
		{
            if (idx < n_vertices) is_control[idx] = true;
        }

        for (auto vh : mesh.vertices()) 
		{
            const int i = vh.idx();
            
            if (is_control[i]) 
			{
                triplets.emplace_back(i, i, 1.0);
                continue;
            }

            double total_weight = 0.0;
            for (auto voh : mesh.voh_range(vh)) 
			{
                const auto he_opp = mesh.opposite_halfedge_handle(voh);
                if (!he_opp.is_valid()) continue;

                const int j = mesh.to_vertex_handle(voh).idx();

                double cot_alpha = 0.0, cot_beta = 0.0;
                
                if (mesh.face_handle(voh).is_valid()) 
				{
                    const auto v0 = vh;
                    const auto v1 = mesh.to_vertex_handle(voh);
                    const auto v2 = mesh.to_vertex_handle(mesh.next_halfedge_handle(voh));

                    const auto& p0 = mesh.point(v0);
                    const auto& p1 = mesh.point(v1);
                    const auto& p2 = mesh.point(v2);

                    const auto e1 = p0 - p2;
                    const auto e2 = p1 - p2;
                    const double dot = OpenMesh::dot(e1, e2);
                    const double cross = OpenMesh::cross(e1, e2).norm();
                    if (cross > 1e-12) cot_alpha = dot / cross;
                }

                if (mesh.face_handle(he_opp).is_valid()) 
				{
                    const auto v0 = vh;
                    const auto v1 = mesh.to_vertex_handle(he_opp);
                    const auto v2 = mesh.to_vertex_handle(mesh.next_halfedge_handle(he_opp));

                    const auto& p0 = mesh.point(v0);
                    const auto& p1 = mesh.point(v1);
                    const auto& p2 = mesh.point(v2);

                    const auto e1 = p0 - p2;
                    const auto e2 = p1 - p2;
                    const double dot = OpenMesh::dot(e1, e2);
                    const double cross = OpenMesh::cross(e1, e2).norm();
                    if (cross > 1e-12) cot_beta = dot / cross;
                }

                const double weight = (cot_alpha + cot_beta) / 2.0;
                if (weight > 0) 
				{
                    triplets.emplace_back(i, j, -weight);
                    total_weight += weight;
                }
            }
            triplets.emplace_back(i, i, total_weight);
        }

        storage.L.resize(n_vertices, n_vertices);
        storage.L.setFromTriplets(triplets.begin(), triplets.end());
        storage.solver.compute(storage.L);
        storage.control_points = control_points;
    }

    Eigen::VectorXd orig_x(n_vertices), orig_y(n_vertices), orig_z(n_vertices);
    for (auto vh : mesh.vertices()) 
	{
        orig_x[vh.idx()] = mesh.point(vh)[0];
        orig_y[vh.idx()] = mesh.point(vh)[1];
        orig_z[vh.idx()] = mesh.point(vh)[2];
    }

    const Eigen::VectorXd delta_x = storage.L * orig_x;
    const Eigen::VectorXd delta_y = storage.L * orig_y;
    const Eigen::VectorXd delta_z = storage.L * orig_z;

    Eigen::VectorXd bx(n_vertices), by(n_vertices), bz(n_vertices);
    for (int i = 0; i < n_vertices; ++i) 
	{
        if (std::find(control_points.begin(), control_points.end(), i) != control_points.end()) 
		{
            bx[i] = changed_vertices[i][0];
            by[i] = changed_vertices[i][1];
            bz[i] = changed_vertices[i][2];
        } 
		else 
		{
            bx[i] = delta_x[i];
            by[i] = delta_y[i];
            bz[i] = delta_z[i];
        }
    }

    const Eigen::VectorXd Xx = storage.solver.solve(bx);
    const Eigen::VectorXd Xy = storage.solver.solve(by);
    const Eigen::VectorXd Xz = storage.solver.solve(bz);

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
    throw std::runtime_error(std::string("[Cotangent] ") + e.what());
}

NODE_DECLARATION_UI(mesh_editing_uniform);
NODE_DECLARATION_UI(mesh_editing_cotangent);

NODE_DEF_CLOSE_SCOPE