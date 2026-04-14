#include <time.h>

#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <cmath>

//#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include <pxr/usd/usdGeom/mesh.h>

#include <Eigen/Core>
#include <Eigen/Eigen>
#include <cfloat>
#include <cstdlib>
#include <unordered_set>
#include <vector>

#include "GCore/Components.h"
#include "GCore/Components/MeshComponent.h"
#include "GCore/GOP.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "nodes/core/def/node_def.hpp"
#include "geom_node_base.h"

/*
** @brief HW4_TutteParameterization
**
** This file presents the basic framework of a "node", which processes inputs
** received from the left and outputs specific variables for downstream nodes to
** use.
** - In the first function, node_declare, you can set up the node's input and
** output variables.
** - The second function, node_exec is the execution part of the node, where we
** need to implement the node's functionality.
** Your task is to fill in the required logic at the specified locations
** within this template, especially in node_exec.
*/

NODE_DEF_OPEN_SCOPE
NODE_DECLARATION_FUNCTION(hw5_param)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input"); 
    
    // Input-2: Weight type selection (0=uniform, 1=floater mean value, 2=cotangent)
    b.add_input<int>("WeightType");
    
    // Input-3: Reference mesh for computing weights (needed for floater weights)
    b.add_input<Geometry>("ReferenceMesh");

    /*
    ** NOTE: You can add more inputs or outputs if necessary. For example, in
    *some cases,
    ** additional information (e.g. other mesh geometry, other parameters) is
    *required to perform
    ** the computation.
    **
    ** Be sure that the input/outputs do not share the same name. You can add
    *one geometry as
    **
    **                b.add_input<Geometry>("Input");
    **
    ** Or maybe you need a value buffer like:
    **
    **                b.add_input<float1Buffer>("Weights");
    */

    // Output-1: Minimal surface with fixed boundary
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_param)
{
    // Get the input from params
    // 从参数中获取输入的几何体
    auto input = params.get_input<Geometry>("Input");
    
    // Get weight type (default to uniform if not provided)
    int weight_type = 0;
    try {
        weight_type = params.get_input<int>("WeightType");
    } catch (...) {
        weight_type = 0; // default to uniform
    }
    
    // Get reference mesh (optional, needed for floater weights)
    Geometry reference_mesh;
    bool has_reference = false;
    try {
        reference_mesh = params.get_input<Geometry>("ReferenceMesh");
        if (reference_mesh.get_component<MeshComponent>()) {
            has_reference = true;
        }
    } catch (...) {
        has_reference = false;
    }

    // (TO BE UPDATED) Avoid processing the node when there is no input
    // 检查输入是否包含网格组件，如果没有则抛出异常
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Minimal Surface: Need Geometry Input.");
        return false;
    }

    /* ----------------------------- Preprocess -------------------------------
    ** Create a halfedge structure (using OpenMesh) for the input mesh. The
    ** half-edge data structure is a widely used data structure in geometric
    ** processing, offering convenient operations for traversing and modifying
    ** mesh elements.
    */
    // 将输入的几何体转换为OpenMesh的半边结构
    // 半边数据结构是几何处理中常用的数据结构，方便遍历和修改网格元素
    auto halfedge_mesh = operand_to_openmesh(&input);
    
    // ---------------- [HW5] Minimal Surface (uniform/floater weights) -------------
    
    // Numerical stability constants
    constexpr double MIN_ANGLE_THRESHOLD = 0.5 * M_PI / 180.0; // 0.5 degrees in radians
    constexpr double MIN_EDGE_LENGTH = 1e-6;
    constexpr double EPSILON = 1e-10;
    
    // Function to compute Floater mean value weights (mean-value coordinates).
    // Formula: w_k = (tan(α_k/2) + tan(β_k/2)) / |v_k - v_i|
    // Where:
    //   v_i is center vertex
    //   v_k is the k-th neighbor
    //   v_{k-1} is previous neighbor of v_k in the 1-ring
    //   v_{k+1} is next neighbor of v_k in the 1-ring
    //   α_k = angle(v_{k-1}, v_i, v_k)
    //   β_k = angle(v_k, v_i, v_{k+1})
    auto compute_floater_weights = [&](const auto* mesh_ptr, int center_old_idx,
                                       const std::vector<int>& neighbor_old_indices,
                                       std::vector<double>& weights) -> bool {
        if (!mesh_ptr) {
            std::cerr << "[Floater] Error: mesh_ptr is null" << std::endl;
            return false;
        }
        auto ch = mesh_ptr->vertex_handle(center_old_idx);
        if (!ch.is_valid()) {
            std::cerr << "[Floater] Error: invalid center vertex handle " << center_old_idx << std::endl;
            return false;
        }

        OpenMesh::Vec3d center = OpenMesh::Vec3d(mesh_ptr->point(ch));
        int degree = static_cast<int>(neighbor_old_indices.size());
        if (degree < 2) {
            std::cerr << "[Floater] Error: degree < 2 (degree=" << degree << ") for vertex " << center_old_idx << std::endl;
            return false;
        }

        weights.assign(degree, 0.0);
        std::vector<double> raw_weights(degree, 0.0);
        double total_weight = 0.0;

        std::cout << "[Floater] Computing weights for vertex " << center_old_idx << " with degree " << degree << std::endl;

        for (int k = 0; k < degree; ++k) {
            int id_prev = neighbor_old_indices[(k - 1 + degree) % degree];
            int id_curr = neighbor_old_indices[k];
            int id_next = neighbor_old_indices[(k + 1) % degree];

            auto vh_prev = mesh_ptr->vertex_handle(id_prev);
            auto vh_curr = mesh_ptr->vertex_handle(id_curr);
            auto vh_next = mesh_ptr->vertex_handle(id_next);
            if (!vh_prev.is_valid() || !vh_curr.is_valid() || !vh_next.is_valid()) {
                std::cerr << "[Floater] Error: invalid neighbor handles for vertex " << center_old_idx << std::endl;
                return false;
            }

            const auto& p_prev = mesh_ptr->point(vh_prev);
            const auto& p_curr = mesh_ptr->point(vh_curr);
            const auto& p_next = mesh_ptr->point(vh_next);

            OpenMesh::Vec3d v1 = OpenMesh::Vec3d(p_prev) - center;
            OpenMesh::Vec3d v2 = OpenMesh::Vec3d(p_curr) - center;
            OpenMesh::Vec3d v3 = OpenMesh::Vec3d(p_next) - center;

            double len1 = std::max(v1.length(), MIN_EDGE_LENGTH);
            double len2 = std::max(v2.length(), MIN_EDGE_LENGTH);
            double len3 = std::max(v3.length(), MIN_EDGE_LENGTH);

            v1 /= len1; v2 /= len2; v3 /= len3;

            double cos_alpha = std::clamp(v1.dot(v2), -1.0, 1.0);
            double cos_beta = std::clamp(v2.dot(v3), -1.0, 1.0);
            double alpha = std::acos(cos_alpha);
            double beta = std::acos(cos_beta);

            alpha = std::max(alpha, MIN_ANGLE_THRESHOLD);
            beta  = std::max(beta, MIN_ANGLE_THRESHOLD);

            double tan_half_alpha = std::tan(alpha * 0.5);
            double tan_half_beta  = std::tan(beta  * 0.5);

            double w = (tan_half_alpha + tan_half_beta) / len2;
            if (!std::isfinite(w) || w <= 0.0) {
                std::cerr << "[Floater] Error: invalid weight w=" << w << " for neighbor " << k << std::endl;
                return false;
            }

            raw_weights[k] = w;
            total_weight += w;
        }

        if (total_weight < EPSILON) {
            std::cerr << "[Floater] Error: total_weight too small (" << total_weight << ") for vertex " << center_old_idx << std::endl;
            return false;
        }
        for (int k = 0; k < degree; ++k) {
            weights[k] = raw_weights[k] / total_weight;
            if (!std::isfinite(weights[k])) {
                std::cerr << "[Floater] Error: normalized weight is NaN for vertex " << center_old_idx << std::endl;
                return false;
            }
        }
        std::cout << "[Floater] Successfully computed weights for vertex " << center_old_idx << std::endl;
        return true;
    };
    
    // [HW5 TODO] Function to compute Cotangent weights
    // Formula: w_j = cot(α_ij) + cot(β_ij)
    // Where:
    //   For edge (i, j), there are two triangles sharing this edge:
    //   - Triangle 1: (i, j, k1)
    //   - Triangle 2: (i, j, k2)
    //   α_ij is the angle at vertex k1 in triangle (i, j, k1), i.e., angle(i, k1, j)
    //   β_ij is the angle at vertex k2 in triangle (i, j, k2), i.e., angle(i, k2, j)
    // 
    // 直观理解：这两个角是 "对着" 边(i,j)的角，即以第三个顶点为顶点的角
    auto compute_cotangent_weights = [&](const auto* mesh_ptr, int center_old_idx,
                                        const std::vector<int>& neighbor_old_indices,
                                        std::vector<double>& weights) -> bool {
        if (!mesh_ptr) {
            std::cerr << "[Cotangent] Error: mesh_ptr is null" << std::endl;
            return false;
        }
        auto ch = mesh_ptr->vertex_handle(center_old_idx);
        if (!ch.is_valid()) {
            std::cerr << "[Cotangent] Error: invalid center vertex handle " << center_old_idx << std::endl;
            return false;
        }

        OpenMesh::Vec3d center = OpenMesh::Vec3d(mesh_ptr->point(ch));
        int degree = static_cast<int>(neighbor_old_indices.size());
        if (degree < 1) {  // Changed from 2 to 1 to handle boundary vertices properly
            std::cerr << "[Cotangent] Error: degree < 1 (degree=" << degree << ") for vertex " << center_old_idx << std::endl;
            return false;
        }

        weights.assign(degree, 0.0);
        std::vector<double> raw_weights(degree, 0.0);

        std::cout << "[Cotangent] Computing weights for vertex " << center_old_idx << " with degree " << degree << std::endl;

        // Efficient approach: iterate through neighbors and find shared edges with their opposite angles
        for (int k = 0; k < degree; ++k) {
            int neighbor_idx = neighbor_old_indices[k];
            auto nh = mesh_ptr->vertex_handle(neighbor_idx);
            if (!nh.is_valid()) {
                std::cerr << "[Cotangent] Error: invalid neighbor handle " << neighbor_idx << std::endl;
                return false;
            }

            // Find the halfedge from center to neighbor
            OpenMesh::HalfedgeHandle he_to_neighbor(-1);
            for (const auto& heh : mesh_ptr->voh_range(ch)) {
                if (mesh_ptr->to_vertex_handle(heh).idx() == neighbor_idx) {
                    he_to_neighbor = heh;
                    break;
                }
            }

            double cot_sum = 0.0;

            if (he_to_neighbor.is_valid()) {
                // Check the face on the left side of the halfedge (center -> neighbor)
                OpenMesh::FaceHandle face1 = mesh_ptr->face_handle(he_to_neighbor);
                if (face1.is_valid()) {
                    // Find the third vertex in this triangle
                    OpenMesh::HalfedgeHandle he_opp = mesh_ptr->next_halfedge_handle(he_to_neighbor);
                    OpenMesh::VertexHandle third_vh = mesh_ptr->to_vertex_handle(he_opp);
                    OpenMesh::Vec3d third_pt = OpenMesh::Vec3d(mesh_ptr->point(third_vh));
                    
                    OpenMesh::Vec3d v_center = center;
                    OpenMesh::Vec3d v_neighbor = OpenMesh::Vec3d(mesh_ptr->point(nh));
                    
                    // Compute the angle at the third vertex between center and neighbor
                    OpenMesh::Vec3d edge1 = v_center - third_pt;  // vector from third to center
                    OpenMesh::Vec3d edge2 = v_neighbor - third_pt;  // vector from third to neighbor
                    
                    double len1 = edge1.norm();
                    double len2 = edge2.norm();
                    
                    if (len1 > MIN_EDGE_LENGTH && len2 > MIN_EDGE_LENGTH) {
                        edge1.normalize();
                        edge2.normalize();
                        
                        double cos_angle = std::clamp(edge1.dot(edge2), -1.0, 1.0);
                        double angle = std::acos(std::abs(cos_angle)); // Use abs to handle precision issues
                        angle = std::max(angle, MIN_ANGLE_THRESHOLD);
                        
                        double sin_angle = std::sin(angle);
                        if (sin_angle > MIN_EDGE_LENGTH) {
                            double cot_angle = std::abs(std::cos(angle) / sin_angle); // Ensure positive weight
                            cot_sum += cot_angle;
                        }
                    }
                }

                // Check the opposite face (on the other side of the edge)
                OpenMesh::HalfedgeHandle he_opp_edge = mesh_ptr->opposite_halfedge_handle(he_to_neighbor);
                OpenMesh::FaceHandle face2 = mesh_ptr->face_handle(he_opp_edge);
                if (face2.is_valid()) {
                    // Find the third vertex in the opposite triangle
                    OpenMesh::HalfedgeHandle he_opp2 = mesh_ptr->next_halfedge_handle(he_opp_edge);
                    OpenMesh::VertexHandle third_vh2 = mesh_ptr->to_vertex_handle(he_opp2);
                    OpenMesh::Vec3d third_pt2 = OpenMesh::Vec3d(mesh_ptr->point(third_vh2));
                    
                    OpenMesh::Vec3d v_center = center;
                    OpenMesh::Vec3d v_neighbor = OpenMesh::Vec3d(mesh_ptr->point(nh));
                    
                    // Compute the angle at the third vertex between center and neighbor
                    OpenMesh::Vec3d edge1 = v_center - third_pt2;  // vector from third to center
                    OpenMesh::Vec3d edge2 = v_neighbor - third_pt2;  // vector from third to neighbor
                    
                    double len1 = edge1.norm();
                    double len2 = edge2.norm();
                    
                    if (len1 > MIN_EDGE_LENGTH && len2 > MIN_EDGE_LENGTH) {
                        edge1.normalize();
                        edge2.normalize();
                        
                        double cos_angle = std::clamp(edge1.dot(edge2), -1.0, 1.0);
                        double angle = std::acos(std::abs(cos_angle));
                        angle = std::max(angle, MIN_ANGLE_THRESHOLD);
                        
                        double sin_angle = std::sin(angle);
                        if (sin_angle > MIN_EDGE_LENGTH) {
                            double cot_angle = std::abs(std::cos(angle) / sin_angle);
                            cot_sum += cot_angle;
                        }
                    }
                }
                // For boundary edges, we only have one adjacent triangle, which is handled above
            }

            raw_weights[k] = cot_sum;
            std::cout << "[Cotangent] Weight for neighbor " << k << " (idx=" << neighbor_idx << "): " << cot_sum << std::endl;
        }
        
        // Normalize weights - only if total weight is positive
        double total_weight = 0.0;
        for (int k = 0; k < degree; ++k) {
            total_weight += raw_weights[k];
        }
        
        if (total_weight <= EPSILON) {
            // Fallback to uniform weights if cotangent weights sum to zero or negative
            for (int k = 0; k < degree; ++k) {
                weights[k] = 1.0 / degree;
            }
            std::cout << "[Cotangent] Warning: total_weight too small (" << total_weight << "), using uniform weights for vertex " << center_old_idx << std::endl;
            return true; // Return true to allow fallback to uniform weights for boundary vertices
        }
        
        for (int k = 0; k < degree; ++k) {
            weights[k] = raw_weights[k] / total_weight;
            if (!std::isfinite(weights[k])) {
                std::cerr << "[Cotangent] Error: normalized weight is NaN for vertex " << center_old_idx << std::endl;
                return false;
            }
        }
        std::cout << "[Cotangent] Successfully computed weights for vertex " << center_old_idx << std::endl;
        return true;
    };

    // 在现有的权重函数之后添加新的权重函数
    auto compute_mean_value_coordinates = [&](const auto* mesh_ptr, int center_old_idx,
                                            const std::vector<int>& neighbor_old_indices,
                                            std::vector<double>& weights) -> bool {
    if (!mesh_ptr) {
        std::cerr << "[MeanValueCoords] Error: mesh_ptr is null" << std::endl;
        return false;
    }
    auto ch = mesh_ptr->vertex_handle(center_old_idx);
    if (!ch.is_valid()) {
        std::cerr << "[MeanValueCoords] Error: invalid center vertex handle " << center_old_idx << std::endl;
        return false;
    }

    OpenMesh::Vec3d center = OpenMesh::Vec3d(mesh_ptr->point(ch));
    int degree = static_cast<int>(neighbor_old_indices.size());
    if (degree < 2) {
        std::cerr << "[MeanValueCoords] Error: degree < 2 (degree=" << degree << ") for vertex " << center_old_idx << std::endl;
        return false;
    }

    weights.assign(degree, 0.0);
    std::vector<double> raw_weights(degree, 0.0);
    double total_weight = 0.0;

    std::cout << "[MeanValueCoords] Computing weights for vertex " << center_old_idx << " with degree " << degree << std::endl;

    // 使用逆时针顺序遍历邻居（确保一致性）
    for (int k = 0; k < degree; ++k) {
        int id_curr = neighbor_old_indices[k];
        int id_next = neighbor_old_indices[(k + 1) % degree];  // 顺时针方向的下一个邻居

        auto vh_curr = mesh_ptr->vertex_handle(id_curr);
        auto vh_next = mesh_ptr->vertex_handle(id_next);
        if (!vh_curr.is_valid() || !vh_next.is_valid()) {
            std::cerr << "[MeanValueCoords] Error: invalid neighbor handles for vertex " << center_old_idx << std::endl;
            return false;
        }

        const auto& p_curr = mesh_ptr->point(vh_curr);
        const auto& p_next = mesh_ptr->point(vh_next);

        // 向量从当前邻居指向中心点，从下一个邻居指向中心点
        OpenMesh::Vec3d v_curr = center - OpenMesh::Vec3d(p_curr);  // 从当前邻居指向中心
        OpenMesh::Vec3d v_next = center - OpenMesh::Vec3d(p_next);  // 从下一个邻居指向中心

        double len_curr = std::max(v_curr.length(), MIN_EDGE_LENGTH);
        double len_next = std::max(v_next.length(), MIN_EDGE_LENGTH);

        v_curr /= len_curr;  // 归一化
        v_next /= len_next;  // 归一化

        // 计算夹角（即该边对应的圆心角的一半）
        double cos_angle = std::clamp(v_curr.dot(v_next), -1.0, 1.0);
        double angle = std::acos(cos_angle);

        // 避免角度过小导致的数值不稳定
        angle = std::max(angle, MIN_ANGLE_THRESHOLD);

        // 计算该邻居的权重 - 使用平均值坐标的经典公式
        // w_k = (tan(α_k/2) + tan(β_k/2)) / ||v_k - v_i||
        // 在这里，α_k 和 β_k 分别是前后两个相邻顶点与当前顶点形成的角
        
        // 对于顶点 id_curr，它对应的角度是 v_next 和 v_prev 之间的夹角
        int id_prev = neighbor_old_indices[(k - 1 + degree) % degree];
        auto vh_prev = mesh_ptr->vertex_handle(id_prev);
        if (!vh_prev.is_valid()) {
            std::cerr << "[MeanValueCoords] Error: invalid prev neighbor handle for vertex " << center_old_idx << std::endl;
            return false;
        }
        const auto& p_prev = mesh_ptr->point(vh_prev);
        OpenMesh::Vec3d v_prev = center - OpenMesh::Vec3d(p_prev);
        double len_prev = std::max(v_prev.length(), MIN_EDGE_LENGTH);
        v_prev /= len_prev;

        // 计算当前邻居对应的两个角
        double cos_alpha = std::clamp(v_prev.dot(v_curr), -1.0, 1.0);
        double cos_beta = std::clamp(v_curr.dot(v_next), -1.0, 1.0);
        double alpha = std::acos(cos_alpha);
        double beta = std::acos(cos_beta);

        alpha = std::max(alpha, MIN_ANGLE_THRESHOLD);
        beta = std::max(beta, MIN_ANGLE_THRESHOLD);

        double tan_half_alpha = std::tan(alpha * 0.5);
        double tan_half_beta = std::tan(beta * 0.5);

        // 平均值坐标权重公式
        double dist_to_neighbor = (OpenMesh::Vec3d(p_curr) - center).length();
        double w = (tan_half_alpha + tan_half_beta) / std::max(dist_to_neighbor, MIN_EDGE_LENGTH);

        if (!std::isfinite(w) || w <= 0.0) {
            std::cerr << "[MeanValueCoords] Error: invalid weight w=" << w << " for neighbor " << k << std::endl;
            return false;
        }

        raw_weights[k] = w;
        total_weight += w;
    }

    if (total_weight < EPSILON) {
        std::cerr << "[MeanValueCoords] Error: total_weight too small (" << total_weight << ") for vertex " << center_old_idx << std::endl;
        return false;
    }

    for (int k = 0; k < degree; ++k) {
        weights[k] = raw_weights[k] / total_weight;
        if (!std::isfinite(weights[k])) {
            std::cerr << "[MeanValueCoords] Error: normalized weight is NaN for vertex " << center_old_idx << std::endl;
            return false;
        }
    }

    std::cout << "[MeanValueCoords] Successfully computed weights for vertex " << center_old_idx << std::endl;
    return true;
    };


    // 说明：OpenMesh 的 vh.idx() 可能不是 0..n-1 连续的（存在删除操作）
    // 我们先把原始 idx 映射为连续的 [0, n_vertices-1]，之后所有矩阵/向量都用新索引
    int n_vertices = static_cast<int>(halfedge_mesh->n_vertices());

    // Build old->new index map
    int max_idx = -1;
    for (const auto& vh : halfedge_mesh->vertices()) {
        int id = vh.idx();
        if (id > max_idx) max_idx = id;
    }
    std::vector<int> old2new(max_idx + 1, -1);
    int new_idx = 0;
    for (const auto& vh : halfedge_mesh->vertices()) {
        old2new[vh.idx()] = new_idx++;
    }
    // new_idx should equal the number of vertices
    if (new_idx != n_vertices) {
        // 修正 n_vertices 以防不一致
        n_vertices = new_idx;
    }

    // Detect boundary vertices using the compact indices
    std::vector<char> is_boundary(n_vertices, 0);
    int boundary_count = 0;
    for (const auto& vh : halfedge_mesh->vertices()) {
        int i = old2new[vh.idx()];
        if (i < 0) continue; // safety
        if (vh.is_boundary()) {
            is_boundary[i] = 1;
            ++boundary_count;
        }
    }
    std::cout << "Total vertices: " << n_vertices << ", Boundary vertices: " << boundary_count << std::endl;
    std::cout << "Weight type: " << weight_type << " (0=uniform, 1=floater, 2=cotangent)" << std::endl;
    std::cout << "Has reference mesh: " << (has_reference ? "true" : "false") << std::endl;

    // 如果没有内部顶点（全部为边界），直接返回原网格
    int interior_count = n_vertices - boundary_count;
    if (interior_count <= 0) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    // 构造稀疏拉普拉斯矩阵（只对内部顶点）
    // 关键改进：边界点不包含在矩阵中，完全固定
    // 内部顶点索引映射：new_index = interior_indices[old2new[vh.idx()]]
    std::vector<int> interior_indices;  // 存储内部顶点的新索引（0到interior_count-1）
    interior_indices.resize(n_vertices, -1);

    int interior_idx = 0;
    for (int i = 0; i < n_vertices; ++i) {
        if (!is_boundary[i]) {
            interior_indices[i] = interior_idx++;
        }
    }

    // 保存边界点的原始坐标（teach文档要求：极小曲面必须完全固定边界点的原始坐标）
    std::vector<OpenMesh::Vec3d> boundary_original_coords;
    boundary_original_coords.resize(n_vertices);
    for (const auto& vh : halfedge_mesh->vertices()) {
        int i = old2new[vh.idx()];
        if (i < 0) continue;
        if (is_boundary[i]) {
            boundary_original_coords[i] = OpenMesh::Vec3d(halfedge_mesh->point(vh));
        }
    }

    // 构建内部顶点的拉普拉斯矩阵和右端项（一次遍历，确保权重一致）
    std::vector<Eigen::Triplet<double>> triplets;
    triplets.reserve(interior_count * 6);
    Eigen::VectorXd bx = Eigen::VectorXd::Zero(interior_count);
    Eigen::VectorXd by = Eigen::VectorXd::Zero(interior_count);
    Eigen::VectorXd bz = Eigen::VectorXd::Zero(interior_count);
    
    // Use reference mesh for weight computation if available and floater weights requested
    std::cout << "[DEBUG] Setting up weight mesh..." << std::endl;
    
    // [CRITICAL FIX] Store the reference mesh OpenMesh object to avoid dangling pointer
    // operand_to_openmesh returns a std::shared_ptr<PolyMesh> that gets destroyed after the statement
    // We need to store it in a variable to keep it alive
    std::shared_ptr<PolyMesh> reference_halfedge_mesh;
    if (weight_type == 1 && has_reference) {
        reference_halfedge_mesh = operand_to_openmesh(&reference_mesh);
        std::cout << "[DEBUG] Reference mesh converted to OpenMesh successfully" << std::endl;
    }
    
    auto* weight_mesh = (weight_type == 1 && has_reference && reference_halfedge_mesh) ? 
                        reference_halfedge_mesh.get() : 
                        halfedge_mesh.get();
    std::cout << "[DEBUG] Weight mesh pointer: " << weight_mesh << std::endl;
    if (weight_mesh) {
        std::cout << "[DEBUG] Weight mesh vertices: " << weight_mesh->n_vertices() << std::endl;
    } else {
        std::cout << "[DEBUG] Weight mesh is null!" << std::endl;
    }

    std::cout << "[DEBUG] Starting to process vertices..." << std::endl;
    int processed_count = 0;
    for (const auto& vh : halfedge_mesh->vertices()) {
        int i_old = vh.idx();
        int i = old2new[i_old];
        if (i < 0) continue;

        // 只处理内部顶点
        if (!is_boundary[i]) {
            processed_count++;
            if (processed_count <= 3) {
                std::cout << "[DEBUG] Processing interior vertex " << i_old << " (new idx: " << i << ")" << std::endl;
            }
            int new_i = interior_indices[i];

            std::vector<int> neighbor_old_indices;  // 邻居的 old idx (vh.idx())
            std::vector<int> neighbor_new_indices;  // 邻居的 compressed new idx
            std::vector<bool> is_boundary_neighbor; // 邻居是否为边界点
            int degree = 0;

            // 遍历所有邻居，保存 old/new idx
            for (const auto& vv : halfedge_mesh->vv_range(vh)) {
                int old_j = vv.idx();
                if (old_j < 0 || old_j >= static_cast<int>(old2new.size())) continue;
                int new_j = old2new[old_j];
                if (new_j < 0) continue;
                neighbor_old_indices.push_back(old_j);
                neighbor_new_indices.push_back(new_j);
                is_boundary_neighbor.push_back(is_boundary[new_j]);
                ++degree;
            }

            if (degree > 0) {
                triplets.emplace_back(new_i, new_i, 1.0);

                if (processed_count <= 3) {
                    std::cout << "[DEBUG] Vertex " << i_old << " has degree " << degree << std::endl;
                    std::cout << "[DEBUG] About to compute weights..." << std::endl;
                }

                // Compute weights based on weight type
                std::vector<double> weights;
                bool use_uniform = true;

                if (weight_type == 1) {
                    // [CRITICAL] Check if neighbor indices are valid in weight_mesh
                    if (weight_mesh != halfedge_mesh.get()) {
                        bool all_valid = true;
                        for (int idx : neighbor_old_indices) {
                            auto test_vh = weight_mesh->vertex_handle(idx);
                            if (!test_vh.is_valid()) {
                                std::cerr << "[ERROR] Neighbor index " << idx << " is INVALID in reference mesh!" << std::endl;
                                all_valid = false;
                                break;
                            }
                        }
                        if (!all_valid) {
                            std::cerr << "[ERROR] Cannot use reference mesh - topology mismatch! Falling back to input mesh." << std::endl;
                            weight_mesh = halfedge_mesh.get();
                        }
                    }
                    std::cout << "[DEBUG] Calling compute_floater_weights for vertex " << i_old << std::endl;
                    // Floater mean value weights
                    use_uniform = !compute_floater_weights(weight_mesh, i_old, neighbor_old_indices, weights);
                    if (use_uniform) {
                        std::cout << "[Warning] Floater weights failed for vertex " << i_old
                                  << ", falling back to uniform weights" << std::endl;
                    } else {
                        std::cout << "[DEBUG] Floater weights computed successfully for vertex " << i_old << std::endl;
                    }
                } else if (weight_type == 2) {
                    std::cout << "[DEBUG] Calling compute_cotangent_weights for vertex " << i_old << std::endl;
                    // Cotangent weights
                    use_uniform = !compute_cotangent_weights(weight_mesh, i_old, neighbor_old_indices, weights);
                    if (use_uniform) {
                        std::cout << "[Warning] Cotangent weights failed for vertex " << i_old
                                  << ", falling back to uniform weights" << std::endl;
                    } else {
                        std::cout << "[DEBUG] Cotangent weights computed successfully for vertex " << i_old << std::endl;
                    }
                }else if (weight_type == 3) {
                    // Mean Value Coordinates
                    use_uniform = !compute_mean_value_coordinates(weight_mesh, i_old, neighbor_old_indices, weights);
                    if (use_uniform) {
                        std::cout << "[Warning] Mean Value Coordinates failed for vertex " << i_old
                                << ", falling back to uniform weights" << std::endl;
                    } else {
                        std::cout << "[DEBUG] Mean Value Coordinates computed successfully for vertex " << i_old << std::endl;
                    }
                }

                // 对每个邻居同时添加矩阵元素和右端项贡献
                for (int k = 0; k < degree; ++k) {
                    double weight = use_uniform ? (1.0 / degree) : weights[k];
                    if (!is_boundary_neighbor[k]) {
                        // 内部邻居：添加到矩阵
                        int neighbor_new = neighbor_new_indices[k];
                        int new_j = interior_indices[neighbor_new];
                        if (new_j >= 0) triplets.emplace_back(new_i, new_j, -weight);
                    } else {
                        // 边界邻居：添加到右端项（使用固定的边界坐标）
                        int old_j = neighbor_old_indices[k];
                        int new_j = neighbor_new_indices[k];
                        const OpenMesh::Vec3d& fixed_coord = boundary_original_coords[new_j];
                        bx[new_i] += weight * fixed_coord[0];
                        by[new_i] += weight * fixed_coord[1];
                        bz[new_i] += weight * fixed_coord[2];
                    }
                }
            }
        }
    }

    std::cout << "[DEBUG] Finished processing " << processed_count << " interior vertices" << std::endl;
    std::cout << "[DEBUG] Building sparse matrix..." << std::endl;
    Eigen::SparseMatrix<double> A(interior_count, interior_count);
    A.setFromTriplets(triplets.begin(), triplets.end());
    std::cout << "[DEBUG] Sparse matrix built successfully" << std::endl;

    

    // 求解 A x = b（三个分量）使用非对称矩阵求解器 BiCGSTAB
    // Floater论文第7节推荐使用Bi-CGSTAB等非对称矩阵求解器
    auto try_solve_once = [&](const Eigen::SparseMatrix<double>& mat, const Eigen::VectorXd& b, Eigen::VectorXd& sol) -> bool {
        Eigen::BiCGSTAB<Eigen::SparseMatrix<double>> local_solver;
        local_solver.setTolerance(1e-10);
        local_solver.setMaxIterations(10000);
        local_solver.compute(mat);
        if (local_solver.info() != Eigen::Success) return false;
        sol = local_solver.solve(b);
        if (local_solver.info() != Eigen::Success) return false;
        for (int k = 0; k < sol.size(); ++k) if (!std::isfinite(sol[k])) return false;
        double maxabs = sol.cwiseAbs().maxCoeff();
        if (!std::isfinite(maxabs) || maxabs > 1e12) return false;
        return true;
    };

    Eigen::VectorXd sol_x, sol_y, sol_z;
    bool solved = try_solve_once(A, bx, sol_x) && try_solve_once(A, by, sol_y) && try_solve_once(A, bz, sol_z);

    if (!solved) {
        // 尝试一系列逐步增大的对角正则化
        std::vector<double> regs = {1e-9, 1e-8, 1e-7, 1e-6, 1e-5};
        for (double eps : regs) {
            Eigen::SparseMatrix<double> Areg = A;
            std::vector<Eigen::Triplet<double>> diag_t;
            diag_t.reserve(interior_count);
            for (int k = 0; k < interior_count; ++k) diag_t.emplace_back(k, k, eps);
            Eigen::SparseMatrix<double> D(interior_count, interior_count);
            D.setFromTriplets(diag_t.begin(), diag_t.end());
            Areg += D;
            if (try_solve_once(Areg, bx, sol_x) && try_solve_once(Areg, by, sol_y) && try_solve_once(Areg, bz, sol_z)) {
                A = std::move(Areg);
                solved = true;
                break;
            }
        }
    }

    if (!solved) {
        throw std::runtime_error("Minimal Surface: numeric solution invalid (NaN/Inf) or factorization failed.");
    }

    // 更新顶点坐标（边界点保持原坐标不变，只更新内部顶点）
    // 关键改进：直接使用求解结果，不做位移钳制
    // teach文档和Floater论文要求：求解结果就是内部顶点的最终绝对坐标
    for (const auto& vh : halfedge_mesh->vertices()) {
        int i_old = vh.idx();
        int i = old2new[i_old];
        if (i < 0) continue;

        if (!is_boundary[i]) {
            int new_i = interior_indices[i];
            OpenMesh::Vec3d target(sol_x[new_i], sol_y[new_i], sol_z[new_i]);
            if (!std::isfinite(target[0]) || !std::isfinite(target[1]) || !std::isfinite(target[2])) {
                std::cerr << "Warning: invalid solution for vertex " << i_old << std::endl;
                continue;
            }
            // 直接使用求解结果，不做位移钳制
            try {
                halfedge_mesh->set_point(vh, OpenMesh::Vec3f(target));
            } catch (...) {
                halfedge_mesh->point(vh)[0] = static_cast<float>(target[0]);
                halfedge_mesh->point(vh)[1] = static_cast<float>(target[1]);
                halfedge_mesh->point(vh)[2] = static_cast<float>(target[2]);
            }
        }
        // 边界点：不更新，保持原坐标
    }

    /* ----------------------------- Postprocess ------------------------------
    ** Convert the minimal surface mesh from the halfedge structure back to
    ** Geometry format as the node's output.
    */
    // 将更新后的半边结构转换回Geometry格式，作为节点的输出
    auto geometry = openmesh_to_operand(halfedge_mesh.get());

    // Set the output of the nodes
    // 设置节点的输出，这个param是Geometry格式的一个类
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(hw5_param);
NODE_DEF_CLOSE_SCOPE