#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include <Eigen/Sparse>
#include <map>
#include <vector>
#include <cmath>

    
NODE_DEF_OPEN_SCOPE

NODE_DECLARATION_FUNCTION(tutte_cotangent)
{
b.add_input<Geometry>("Input");
b.add_input<Geometry>("ReferenceMesh");
b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(tutte_cotangent)
{
    auto input = params.get_input<Geometry>("Input");
    auto refGeom = params.get_input<Geometry>("ReferenceMesh");
    if (!input.get_component<MeshComponent>() || !refGeom.get_component<MeshComponent>())
    {
        throw std::runtime_error("TutteCotangent: Need both geometry inputs.");
    }
    auto halfedge_mesh = operand_to_openmesh(&input);
    auto ref_halfedge_mesh = operand_to_openmesh(&refGeom);

    using Mesh = decltype(halfedge_mesh)::element_type;
    auto& mesh = *halfedge_mesh;
    auto& ref_mesh = *ref_halfedge_mesh;

    std::vector<bool> is_boundary(mesh.n_vertices(), false);
    for (auto vh : mesh.vertices())
    {
        is_boundary[vh.idx()] = mesh.is_boundary(vh);
    }

    using SpMat = Eigen::SparseMatrix<double>;
    using Triplet = Eigen::Triplet<double>;
    std::vector<Triplet> tripletList;
    tripletList.reserve(mesh.n_vertices() * 7);

    Eigen::VectorXd bx(mesh.n_vertices()), by(mesh.n_vertices()), bz(mesh.n_vertices());
    bx.setZero(); 
    by.setZero(); 
    bz.setZero();

    for (auto vh : mesh.vertices())
    {
        int i = vh.idx();
        if (is_boundary[i])
        {
            tripletList.emplace_back(i, i, 1.0);
            auto p = mesh.point(vh);
            bx(i) = p[0];
            by(i) = p[1];
            bz(i) = p[2];
        }
        else
        {
            double w_sum = 0.0;
            for (auto vv : mesh.vv_range(vh))
            {
                int j = vv.idx();
                double w_ij = 0.0;
                for (auto fh : ref_mesh.vf_range(ref_mesh.vertex_handle(i)))
                {
                    bool face_has_j = false;
                    for (auto fvh : ref_mesh.fv_range(fh))
                    {
                        if (fvh.idx() == j)
                        {
                            face_has_j = true;
                            break;
                        }
                    }
                    if (!face_has_j) 
                    {
                        continue;
                    }
                    std::vector<int> fverts;
                    for (auto fvh : ref_mesh.fv_range(fh))
                    {
                        fverts.push_back(fvh.idx());
                    }
                    if (fverts.size() != 3)
                    {
                        continue;
                    }
                    int k = -1;
                    for (auto idxv : fverts)
                    {
                        if (idxv != i && idxv != j)
                        {
                            k = idxv;
                            break;
                        }
                    }
                    if (k < 0)
                    {
                        continue;
                    }
                    auto pi = ref_mesh.point(ref_mesh.vertex_handle(i));
                    auto pj = ref_mesh.point(ref_mesh.vertex_handle(j));
                    auto pk = ref_mesh.point(ref_mesh.vertex_handle(k));

                    {
                        auto ki = pi - pk;
                        auto kj = pj - pk;
                        double len_ki = ki.norm();
                        double len_kj = kj.norm();
                        if (len_ki < 1e-12 || len_kj < 1e-12)
                        {
                            continue;
                        }
                        auto ki_n = ki / float(len_ki);
                        auto kj_n = kj / float(len_kj);
                        double dot_val = ki_n | kj_n;
                        double cross_val = (ki_n % kj_n).norm();
                        double cot_alpha = (std::fabs(cross_val) < 1e-12)
                                           ? 0.0
                                           : (dot_val / cross_val);
                        w_ij += cot_alpha;
                    }
                } 

                if (std::fabs(w_ij) > 1e-14)
                {
                    w_sum += w_ij;
                    tripletList.emplace_back(i, j, -w_ij);
                }
            }

            tripletList.emplace_back(i, i, w_sum);
        }
    }

    SpMat L(mesh.n_vertices(), mesh.n_vertices());
    L.setFromTriplets(tripletList.begin(), tripletList.end());

    Eigen::SparseLU<SpMat> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success)
    {
        throw std::runtime_error("TutteCotangent: decomposition failed.");
    }
    auto Xx = solver.solve(bx);
    auto Xy = solver.solve(by);
    auto Xz = solver.solve(bz);

    for (auto vh : mesh.vertices())
    {
        int i = vh.idx();
        auto &p = mesh.point(vh);
        p[0] = float(Xx(i));
        p[1] = float(Xy(i));
        p[2] = float(Xz(i));
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_FUNCTION(tutte_uniform)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(tutte_uniform)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) 
    {
        throw std::runtime_error("TutteUniform: Need Geometry Input.");
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

    Eigen::VectorXd bx(mesh.n_vertices()), by(mesh.n_vertices()), bz(mesh.n_vertices());
    bx.setZero(); by.setZero(); bz.setZero();

    for (auto vh : mesh.vertices()) 
    {
        int i = vh.idx();
        if (is_boundary[i]) 
        {
            tripletList.emplace_back(i, i, 1.0);
            auto p = mesh.point(vh);
            bx(i) = p[0];
            by(i) = p[1];
            bz(i) = p[2];
        } 
        else 
        {
            int valence = 0;
            for (auto vv : mesh.vv_range(vh)) 
            { 
                valence++; 
            }
            tripletList.emplace_back(i, i, (double)valence);
            for (auto vv : mesh.vv_range(vh)) 
            {
                tripletList.emplace_back(i, vv.idx(), -1.0);
            }
        }
    }

    SpMat L(mesh.n_vertices(), mesh.n_vertices());
    L.setFromTriplets(tripletList.begin(), tripletList.end());

    Eigen::SparseLU<SpMat> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success) 
    {
        throw std::runtime_error("TutteUniform: decomposition failed.");
    }

    auto Xx = solver.solve(bx);
    auto Xy = solver.solve(by);
    auto Xz = solver.solve(bz);

    for (auto vh : mesh.vertices()) 
    {
        int i = vh.idx();
        auto &p = mesh.point(vh);
        p[0] = float(Xx(i));
        p[1] = float(Xy(i));
        p[2] = float(Xz(i));
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}


NODE_DECLARATION_FUNCTION(tutte_shape_preserving)
{
    b.add_input<Geometry>("Input");
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(tutte_shape_preserving)
{
    auto input = params.get_input<Geometry>("Input");
    if (!input.get_component<MeshComponent>()) 
    {
        throw std::runtime_error("TutteShapePreserving: Need Geometry Input.");
    }
    auto halfedge_mesh = operand_to_openmesh(&input);
    using Mesh = decltype(halfedge_mesh)::element_type;
    auto& mesh = *halfedge_mesh;

    std::vector<bool> is_boundary(mesh.n_vertices(), false);
    for (auto vh : mesh.vertices()) 
    {
        is_boundary[vh.idx()] = mesh.is_boundary(vh);
    }

    using SpMat = Eigen::SparseMatrix<double>;
    using Triplet = Eigen::Triplet<double>;
    std::vector<Triplet> tripletList;
    tripletList.reserve(mesh.n_vertices() * 7);

    Eigen::VectorXd bx(mesh.n_vertices()), by(mesh.n_vertices()), bz(mesh.n_vertices());
    bx.setZero(); by.setZero(); bz.setZero();

    for (auto vh : mesh.vertices()) 
    {
        int i = vh.idx();
        if (is_boundary[i]) 
        {
            tripletList.emplace_back(i, i, 1.0);
            auto p = mesh.point(vh);
            bx(i) = p[0];
            by(i) = p[1];
            bz(i) = p[2];
        } 
        else 
        {
            std::vector<Mesh::VertexHandle> neighbors;
            for (auto vv : mesh.vv_range(vh)) 
            {
                neighbors.push_back(vv);
            }
            int di = neighbors.size();

            std::vector<OpenMesh::Vec2f> local_coords;
            OpenMesh::Vec2f center(0.0f, 0.0f);
            local_coords.push_back(center);

            OpenMesh::Vec3f edge0 = mesh.point(neighbors[0]) - mesh.point(vh);
            float len0 = edge0.norm();
            local_coords.emplace_back(len0, 0.0f);

            float total_angle = 0.0f;
            for (int k = 1; k < di; ++k) 
            {
                OpenMesh::Vec3f vec_prev = mesh.point(neighbors[k-1]) - mesh.point(vh);
                OpenMesh::Vec3f vec_curr = mesh.point(neighbors[k]) - mesh.point(vh);
                float angle = acos(vec_prev.normalized().dot(vec_curr.normalized()));
                total_angle += angle;

                float len = (mesh.point(neighbors[k]) - mesh.point(vh)).norm();
                float theta = 2 * M_PI * total_angle / (2 * M_PI);
                local_coords.emplace_back(len * cos(theta), len * sin(theta));
            }

            std::map<int, double> weights;
            double sum_weights = 0.0;

            for (int k = 0; k < di; ++k) 
            {
                int prev_k = (k == 0) ? di-1 : k-1;
                int next_k = (k+1) % di;

                OpenMesh::Vec2f p_prev = local_coords[prev_k + 1];
                OpenMesh::Vec2f p_curr = local_coords[k + 1];
                OpenMesh::Vec2f p_next = local_coords[next_k + 1];

                OpenMesh::Vec2f vec_prev = p_prev - center;
                OpenMesh::Vec2f vec_curr = p_curr - center;
                float cross_prev = vec_prev[0] * vec_curr[1] - vec_prev[1] * vec_curr[0];
                float area_prev = 0.5f * std::abs(cross_prev);
                
                OpenMesh::Vec2f vec_curr_next = p_curr - center;
                OpenMesh::Vec2f vec_next = p_next - center;
                float cross_curr = vec_curr_next[0] * vec_next[1] - vec_curr_next[1] * vec_next[0];
                float area_curr = 0.5f * std::abs(cross_curr);

                weights[neighbors[k].idx()] = (area_prev + area_curr);
                sum_weights += (area_prev + area_curr);
            }

            if (sum_weights < 1e-6) 
            {
                for (auto& pair : weights) 
                {
                    pair.second = 1.0 / di;
                }
            } 
            else 
            {
                for (auto& pair : weights) 
                {
                    pair.second /= sum_weights;
                }
            }

            tripletList.emplace_back(i, i, 1.0);
            for (auto& pair : weights) 
            {
                tripletList.emplace_back(i, pair.first, -pair.second);
            }
        }
    }

    SpMat L(mesh.n_vertices(), mesh.n_vertices());
    L.setFromTriplets(tripletList.begin(), tripletList.end());

    Eigen::SparseLU<SpMat> solver;
    solver.compute(L);
    if (solver.info() != Eigen::Success) 
    {
        throw std::runtime_error("TutteShapePreserving: decomposition failed.");
    }

    auto Xx = solver.solve(bx);
    auto Xy = solver.solve(by);
    auto Xz = solver.solve(bz);

    for (auto vh : mesh.vertices()) 
    {
        int i = vh.idx();
        auto &p = mesh.point(vh);
        p[0] = float(Xx(i));
        p[1] = float(Xy(i));
        p[2] = float(Xz(i));
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(tutte_uniform);
NODE_DECLARATION_UI(tutte_cotangent);
NODE_DECLARATION_UI(tutte_shape_preserving);

NODE_DEF_CLOSE_SCOPE

