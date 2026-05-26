#include <time.h>

#include <omp.h>

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <Eigen/SparseLU>
#include <Eigen/SVD>
#include <cmath>
#include <limits>

#include "GCore/Components.h"
#include "GCore/Components/MeshComponent.h"
#include "GCore/GOP.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include "nodes/core/def/node_def.hpp"
#include "geom_node_base.h"

/*
** @brief HW6_ARAP_Parameterization
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
NODE_DECLARATION_FUNCTION(hw6_arap)
{
    // Input: Original 3D mesh (with boundary)
    b.add_input<Geometry>("Input");
    
    // Input: Initial parameterization from HW5 (optional, Geometry type)
    // This is the output of hw5_param node, which contains parameterized mesh
    // If not provided, will use simple initialization
    b.add_input<Geometry>("InitialParam");
    
    // Input: Maximum number of iterations (default: 10)
    b.add_input<int>("MaxIterations").min(1).max(100).default_val(10);
    
    // Input: Whether to normalize UV coordinates to [0, 1] range (default: true)
    // If true, UV coordinates will be normalized to [0, 1] for proper texture mapping
    b.add_input<bool>("NormalizeUV");
    
    // Input: Whether to use ASAP (As-Similar-As-Possible) instead of ARAP (default: false)
    // If true, uses ASAP which allows similarity transformations (rotation + scaling)
    // If false, uses ARAP which only allows rigid rotations
    b.add_input<bool>("UseASAP");
    
    // Input: Hybrid blending parameter lambda (>=0).
    // lambda = 0 -> pure ASAP (LSCM); lambda -> +inf -> pure ARAP (rigid)
    // UI slider range: [0, 100000], where large values approximate pure ARAP
    // Note: lambda=0 is pure ASAP, lambda>=10000 is practically pure ARAP
    b.add_input<float>("Lambda").min(0.0f).max(100000.0f).default_val(0.0f);
    
    // Input: Whether to enable parallel computation using OpenMP (default: true)
    // If true, uses multi-threading for local phase computation
    // If false, uses single-threaded execution
    b.add_input<bool>("EnableParallel");
    
    // Input: Whether to enable debug logging (default: false)
    // If true, outputs detailed debug information during iteration
    // If false, only outputs essential progress information
    b.add_input<bool>("EnableDebugLog");

    // Output1: UV coordinates computed by ARAP/ASAP algorithm
    b.add_output<std::vector<glm::vec2>>("OutputUV");
    
    // Output2: Parameterized 3D mesh (UV coordinates applied to vertices with z=0)
    b.add_output<Geometry>("OutputMesh");
}

NODE_EXECUTION_FUNCTION(hw6_arap)
{
    // Get input parameters
    auto input = params.get_input<Geometry>("Input");
    
    // Get initial parameterization from HW5 (optional, Geometry type)
    // This is the output of hw5_param node
    bool has_initial_param = false;
    std::vector<glm::vec2> initial_uv;
    try {
        auto initial_param = params.get_input<Geometry>("InitialParam");
        if (initial_param.get_component<MeshComponent>()) {
            has_initial_param = true;
            std::cout << "[ARAP] Using initial parameterization from HW5" << std::endl;
            
            // Extract UV coordinates from the initial parameterization
            // The hw5_param node outputs a Geometry with parameterized 3D coordinates
            // We can use the (x, y) coordinates as initial UV
            auto initial_halfedge_mesh = operand_to_openmesh(&initial_param);
            int n_initial_vertices = static_cast<int>(initial_halfedge_mesh->n_vertices());
            
            initial_uv.resize(n_initial_vertices);
            for (int i = 0; i < n_initial_vertices; ++i) {
                auto vh = initial_halfedge_mesh->vertex_handle(i);
                OpenMesh::Vec3f p = initial_halfedge_mesh->point(vh);
                initial_uv[i] = glm::vec2(p[0], p[1]);  // Use (x, y) as UV
            }
            
            std::cout << "[ARAP] Extracted " << n_initial_vertices << " UV coordinates from HW5 output" << std::endl;
        }
    } catch (...) {
        std::cout << "[ARAP] No initial parameterization provided, will use simple initialization" << std::endl;
    }
    
    // Get maximum number of iterations (default: 10)
    int max_iterations = 10;
    try {
        max_iterations = params.get_input<int>("MaxIterations");
    } catch (...) {
        max_iterations = 10;
    }
    
    // Get whether to normalize UV coordinates (default: true)
    bool normalize_uv = true;
    try {
        normalize_uv = params.get_input<bool>("NormalizeUV");
    } catch (...) {
        normalize_uv = true;  // default to true
    }
    
    // Get whether to use ASAP (default: false, use ARAP)
    bool use_asap = false;
    try {
        use_asap = params.get_input<bool>("UseASAP");
    } catch (...) {
        use_asap = false;  // default to ARAP
    }
    
    // Get hybrid blending parameter lambda (default: 0 -> pure ASAP)
    float lambda_param = 0.0f;
    try {
        lambda_param = params.get_input<float>("Lambda");
    } catch (...) {
        lambda_param = 0.0f;
    }
    
    // Get whether to enable parallel computation (default: true)
    bool enable_parallel = true;
    try {
        enable_parallel = params.get_input<bool>("EnableParallel");
    } catch (...) {
        enable_parallel = true;  // default to true
    }
    
    // Get whether to enable debug logging (default: false)
    bool enable_debug_log = false;
    try {
        enable_debug_log = params.get_input<bool>("EnableDebugLog");
    } catch (...) {
        enable_debug_log = false;  // default to false
    }

    std::cout << "[ARAP] Using " << (use_asap ? "ASAP" : "ARAP") << " parameterization" 
              << ", lambda=" << lambda_param 
              << ", parallel=" << (enable_parallel ? "enabled" : "disabled")
              << ", debug_log=" << (enable_debug_log ? "enabled" : "disabled")
              << std::endl;

    // Check if input is valid
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Need Geometry Input.");
    }

    /* ----------------------------- 预处理 -------------------------------
    ** 创建OpenMesh半边结构用于网格遍历和修改
    ** 半边数据结构是几何处理中常用的数据结构
    */
    auto halfedge_mesh = operand_to_openmesh(&input);
    
    // 数值稳定性常量
    constexpr double MIN_ANGLE_THRESHOLD = 0.5 * M_PI / 180.0;  // 0.5度（弧度）
    constexpr double MAX_ANGLE_THRESHOLD = 179.5 * M_PI / 180.0;  // 179.5度（弧度），防止tan(π/2)趋向无穷大
    constexpr double MIN_EDGE_LENGTH = 1e-6;
    constexpr double EPSILON = 1e-10;
    // 用于判断局部2x2矩阵退化的阈值（比EPSILON更宽松，避免过多跳过正常三角形）
    constexpr double DET_EPS = 1e-8;
    // 惩罚法固定顶点的权重
    constexpr double PENALTY_WEIGHT = 1e8;
    
    /* ==================== Step 1: Initialization ====================
    ** Use initial UV coordinates from HW5 parameterization if provided
    ** Otherwise, use simple initialization (project to XY plane)
    */
    
    // Get mesh information
    int n_vertices = static_cast<int>(halfedge_mesh->n_vertices());
    int n_faces = static_cast<int>(halfedge_mesh->n_faces());
    
    std::cout << "[ARAP] Mesh info: vertices=" << n_vertices << ", faces=" << n_faces << std::endl;
    
    // Initialize UV coordinates
    std::vector<glm::vec2> uv_coords(n_vertices);
    
    if (has_initial_param && static_cast<int>(initial_uv.size()) == n_vertices) {
        // Use initial UV from HW5 parameterization
        std::cout << "[ARAP] Using initial UV from HW5 parameterization" << std::endl;
        
        // Check for NaN in initial UV
        bool has_nan = false;
        for (int i = 0; i < n_vertices; ++i) {
            if (std::isnan(initial_uv[i].x) || std::isnan(initial_uv[i].y)) {
                std::cerr << "[ARAP ERROR] NaN detected in initial UV at vertex " << i << std::endl;
                has_nan = true;
            }
        }
        
        if (has_nan) {
            throw std::runtime_error("Initial UV from HW5 contains NaN values");
        }
        
        uv_coords = initial_uv;
        
        // Log UV range for debugging
        float min_u = std::numeric_limits<float>::max();
        float max_u = -std::numeric_limits<float>::max();
        float min_v = std::numeric_limits<float>::max();
        float max_v = -std::numeric_limits<float>::max();
        for (const auto& uv : uv_coords) {
            min_u = std::min(min_u, uv.x);
            max_u = std::max(max_u, uv.x);
            min_v = std::min(min_v, uv.y);
            max_v = std::max(max_v, uv.y);
        }
        std::cout << "[ARAP] Initial UV range: u=[" << min_u << ", " << max_u << "], v=[" << min_v << ", " << max_v << "]" << std::endl;
        
    } else {
        // Simple initialization: project to XY plane
        std::cout << "[ARAP] Using simple initialization (project to XY plane)" << std::endl;
        
        for (int i = 0; i < n_vertices; ++i) {
            auto vh = halfedge_mesh->vertex_handle(i);
            OpenMesh::Vec3f p = halfedge_mesh->point(vh);
            uv_coords[i] = glm::vec2(p[0], p[1]);
        }
        
        // Normalize to [0, 1] range
        float min_x = std::numeric_limits<float>::max();
        float max_x = -std::numeric_limits<float>::max();
        float min_y = std::numeric_limits<float>::max();
        float max_y = -std::numeric_limits<float>::max();
        for (const auto& uv : uv_coords) {
            min_x = std::min(min_x, uv.x);
            max_x = std::max(max_x, uv.x);
            min_y = std::min(min_y, uv.y);
            max_y = std::max(max_y, uv.y);
        }
        
        float scale_x = (max_x - min_x) > EPSILON ? 1.0f / (max_x - min_x) : 1.0f;
        float scale_y = (max_y - min_y) > EPSILON ? 1.0f / (max_y - min_y) : 1.0f;
        
        for (int i = 0; i < n_vertices; ++i) {
            uv_coords[i].x = (uv_coords[i].x - min_x) * scale_x;
            uv_coords[i].y = (uv_coords[i].y - min_y) * scale_y;
        }
        
        std::cout << "[ARAP] Simple initialization completed, UV range: [0, 1]" << std::endl;
    }
    
    // Identify boundary and interior vertices
    std::vector<int> boundary_vertices;
    std::vector<int> interior_vertices;
    std::vector<bool> is_boundary(n_vertices, false);
    
    for (int i = 0; i < n_vertices; ++i) {
        auto vh = halfedge_mesh->vertex_handle(i);
        if (halfedge_mesh->is_boundary(vh)) {
            is_boundary[i] = true;
            boundary_vertices.push_back(i);
        } else {
            interior_vertices.push_back(i);
        }
    }
    
    int n_boundary = static_cast<int>(boundary_vertices.size());
    int n_interior = static_cast<int>(interior_vertices.size());
    
    std::cout << "[ARAP] Boundary vertices=" << n_boundary << ", interior vertices=" << n_interior << std::endl;
    
    if (n_boundary < 3) {
        throw std::runtime_error("Mesh must have at least 3 boundary vertices");
    }
    
    /* ==================== Step 2: ARAP Local-Global Iteration ====================
    ** ARAP energy function: E(u, L) = sum_t A_t * ||J_t(u) - L_t||_F^2
    ** where:
    **   - A_t is the area of triangle t
    **   - J_t(u) is the Jacobian matrix of triangle t
    **   - L_t is the local transformation matrix (rotation matrix)
    **
    ** Algorithm flow:
    ** 1. Local phase: Fix u, compute optimal rotation matrix L_t for each triangle (via SVD)
    ** 2. Global phase: Fix L_t, update u (solve sparse linear system)
    ** 3. Iterate until convergence
    */
    
    // 预计算三角形信息
    struct TriangleInfo {
        int v0, v1, v2;           // 顶点索引
        double area;              // 三角形面积
        OpenMesh::Vec3f e0, e1;   // 3D边向量（从v0出发）
        double cot0, cot1, cot2;  // 余切权重
        OpenMesh::Vec3f normal;     // 三角形法线
        OpenMesh::Vec3f basis_x, basis_y;  // 局部2D坐标系基向量
        glm::vec2 x0_local, x1_local, x2_local;  // 顶点在局部坐标系中的2D坐标
    };
    
    std::vector<TriangleInfo> triangles;
    
    for (int i = 0; i < n_faces; ++i) {
        auto fh = halfedge_mesh->face_handle(i);
        
        // 获取三角形的三个顶点
        auto fv_it = halfedge_mesh->fv_begin(fh);
        int v0 = fv_it->idx(); ++fv_it;
        int v1 = fv_it->idx(); ++fv_it;
        int v2 = fv_it->idx();
        
        auto vh0 = halfedge_mesh->vertex_handle(v0);
        auto vh1 = halfedge_mesh->vertex_handle(v1);
        auto vh2 = halfedge_mesh->vertex_handle(v2);
        
        OpenMesh::Vec3f p0 = halfedge_mesh->point(vh0);
        OpenMesh::Vec3f p1 = halfedge_mesh->point(vh1);
        OpenMesh::Vec3f p2 = halfedge_mesh->point(vh2);
        
        // 计算三角形面积
        OpenMesh::Vec3f edge1 = p1 - p0;
        OpenMesh::Vec3f edge2 = p2 - p0;
        double area = 0.5 * (edge1 % edge2).length();
        
        // 计算余切权重
        // cot(θ) = (a·b) / |a×b|
        OpenMesh::Vec3f e01 = p1 - p0;
        OpenMesh::Vec3f e12 = p2 - p1;
        OpenMesh::Vec3f e20 = p0 - p2;
        
        double len01 = static_cast<double>(e01.length());
        double len12 = static_cast<double>(e12.length());
        double len20 = static_cast<double>(e20.length());
        
        double cot0 = 0.0, cot1 = 0.0, cot2 = 0.0;
        
        if (len01 > MIN_EDGE_LENGTH && len20 > MIN_EDGE_LENGTH) {
            double cos_angle0 = (-e01).dot(e20) / (len01 * len20);
            if (cos_angle0 < -1.0) cos_angle0 = -1.0;
            if (cos_angle0 > 1.0) cos_angle0 = 1.0;
            double angle0 = std::acos(cos_angle0);
            // 使用std::clamp同时限制上下限，防止cot(0)趋向无穷大
            angle0 = std::clamp(angle0, MIN_ANGLE_THRESHOLD, MAX_ANGLE_THRESHOLD);
            cot0 = std::cos(angle0) / std::sin(angle0);
            // 钳制余切权重为非负
            cot0 = std::max(cot0, 0.0);
        }
        
        if (len01 > MIN_EDGE_LENGTH && len12 > MIN_EDGE_LENGTH) {
            double cos_angle1 = (-e01).dot(e12) / (len01 * len12);
            if (cos_angle1 < -1.0) cos_angle1 = -1.0;
            if (cos_angle1 > 1.0) cos_angle1 = 1.0;
            double angle1 = std::acos(cos_angle1);
            // 使用std::clamp同时限制上下限，防止cot(0)趋向无穷大
            angle1 = std::clamp(angle1, MIN_ANGLE_THRESHOLD, MAX_ANGLE_THRESHOLD);
            cot1 = std::cos(angle1) / std::sin(angle1);
            // 钳制余切权重为非负
            cot1 = std::max(cot1, 0.0);
        }
        
        if (len12 > MIN_EDGE_LENGTH && len20 > MIN_EDGE_LENGTH) {
            double cos_angle2 = (-e12).dot(e20) / (len12 * len20);
            if (cos_angle2 < -1.0) cos_angle2 = -1.0;
            if (cos_angle2 > 1.0) cos_angle2 = 1.0;
            double angle2 = std::acos(cos_angle2);
            // 使用std::clamp同时限制上下限，防止cot(0)趋向无穷大
            angle2 = std::clamp(angle2, MIN_ANGLE_THRESHOLD, MAX_ANGLE_THRESHOLD);
            cot2 = std::cos(angle2) / std::sin(angle2);
            // 钳制余切权重为非负
            cot2 = std::max(cot2, 0.0);
        }
        
        // 构建局部2D坐标系
        // 以v0为原点，e0为X轴，法线叉乘e0为Y轴
        OpenMesh::Vec3f normal = (p1 - p0) % (p2 - p0);
        double normal_len = normal.length();
        if (normal_len < MIN_EDGE_LENGTH) {
            normal = OpenMesh::Vec3f(0, 0, 1);
        } else {
            normal /= static_cast<float>(normal_len);
        }
        
        // X轴：归一化的e0
        OpenMesh::Vec3f basis_x = edge1;
        double basis_x_len = basis_x.length();
        if (basis_x_len > MIN_EDGE_LENGTH) {
            basis_x /= static_cast<float>(basis_x_len);
        }
        
        // Y轴：法线叉乘X轴
        OpenMesh::Vec3f basis_y = normal % basis_x;
        double basis_y_len = basis_y.length();
        if (basis_y_len > MIN_EDGE_LENGTH) {
            basis_y /= static_cast<float>(basis_y_len);
        }
        
        // 将三个顶点投影到局部2D坐标系
        auto project_to_local = [&](const OpenMesh::Vec3f& p) -> glm::vec2 {
            OpenMesh::Vec3f vec = p - p0;
            float x = vec.dot(basis_x);
            float y = vec.dot(basis_y);
            return glm::vec2(x, y);
        };
        
        glm::vec2 x0_local = project_to_local(p0);
        glm::vec2 x1_local = project_to_local(p1);
        glm::vec2 x2_local = project_to_local(p2);
        
        triangles.push_back({v0, v1, v2, area, edge1, edge2, cot0, cot1, cot2, 
                         normal, basis_x, basis_y, x0_local, x1_local, x2_local});
    }
    
    // 构建全局线性系统的系数矩阵（预分解）
    // 矩阵形式：sum_{(i,j)∈E} cot(θ_ij) * (u_i - u_j - L_t * (x_i - x_j)) = 0
    // 其中L_t是包含边(i,j)的三角形的最优旋转矩阵
    
    // 选择固定点以避免平凡解
    // ARAP只需要1个固定点来消除平移歧义性（拉普拉斯矩阵零空间维度为1）
    // ASAP需要2个固定点来防止坍缩（允许缩放，scale=0是最优解）
    
    int fixed_idx1 = -1;
    int fixed_idx2 = -1;
    
    if (use_asap) {
        // ASAP模式：选择距离最远的两个顶点作为固定点
        // 这样可以最大程度减小数值求解时的杠杆效应，参数化畸变分布更均匀
        double max_distance = 0.0;
        
        for (size_t i = 0; i < interior_vertices.size(); ++i) {
            int idx1 = interior_vertices[i];
            auto vh1 = halfedge_mesh->vertex_handle(idx1);
            OpenMesh::Vec3f p1 = halfedge_mesh->point(vh1);
            
            for (size_t j = i + 1; j < interior_vertices.size(); ++j) {
                int idx2 = interior_vertices[j];
                auto vh2 = halfedge_mesh->vertex_handle(idx2);
                OpenMesh::Vec3f p2 = halfedge_mesh->point(vh2);
                
                double distance = (p1 - p2).length();
                if (distance > max_distance) {
                    max_distance = distance;
                    fixed_idx1 = idx1;
                    fixed_idx2 = idx2;
                }
            }
        }
        
        // 如果内部顶点不足2个，则从边界顶点中选择
        if (fixed_idx1 < 0 || fixed_idx2 < 0) {
            max_distance = 0.0;
            for (size_t i = 0; i < boundary_vertices.size(); ++i) {
                int idx1 = boundary_vertices[i];
                auto vh1 = halfedge_mesh->vertex_handle(idx1);
                OpenMesh::Vec3f p1 = halfedge_mesh->point(vh1);
                
                for (size_t j = i + 1; j < boundary_vertices.size(); ++j) {
                    int idx2 = boundary_vertices[j];
                    auto vh2 = halfedge_mesh->vertex_handle(idx2);
                    OpenMesh::Vec3f p2 = halfedge_mesh->point(vh2);
                    
                    double distance = (p1 - p2).length();
                    if (distance > max_distance) {
                        max_distance = distance;
                        fixed_idx1 = idx1;
                        fixed_idx2 = idx2;
                    }
                }
            }
        }
        
        std::cout << "[ARAP] Fixed vertices (ASAP): " << fixed_idx1 << " and " << fixed_idx2 
                  << " (distance=" << max_distance << ")" << std::endl;
    } else {
        // ARAP模式：选择拓扑中心点（度数最高的内部顶点）作为固定点
        // 只需要1个固定点来消除平移歧义性
        int max_degree = -1;
        
        if (n_interior > 0) {
            // 在内部顶点中选择度数最高的作为固定点
            for (int idx : interior_vertices) {
                auto vh = halfedge_mesh->vertex_handle(idx);
                int degree = static_cast<int>(halfedge_mesh->valence(vh));
                if (degree > max_degree) {
                    max_degree = degree;
                    fixed_idx1 = idx;
                }
            }
        } else {
            // 如果没有内部顶点，选择边界顶点中度数最高的
            for (int idx : boundary_vertices) {
                auto vh = halfedge_mesh->vertex_handle(idx);
                int degree = static_cast<int>(halfedge_mesh->valence(vh));
                if (degree > max_degree) {
                    max_degree = degree;
                    fixed_idx1 = idx;
                }
            }
        }
        
        std::cout << "[ARAP] Fixed vertex (ARAP): " << fixed_idx1 << " (degree=" << max_degree << ")" << std::endl;
    }
    
    // 构建全局线性系统（对所有顶点，包括边界和内部）
    // ARAP是自由边界参数化，所有顶点都应该参与优化
    // 只固定2个顶点避免平凡解（所有点坍缩到同一点）
    Eigen::SparseMatrix<double> A_global(n_vertices, n_vertices);
    std::vector<Eigen::Triplet<double>> triplets_global;
    
    // 首先构建拉普拉斯矩阵（仅拓扑结构，不依赖L_t）
    // 对所有顶点（包括边界和内部）构建
    for (int i = 0; i < n_vertices; ++i) {
        auto vh = halfedge_mesh->vertex_handle(i);

        // 获取邻居顶点
        std::vector<int> neighbors;
        std::vector<double> cot_weights;

        for (auto heh : halfedge_mesh->voh_range(vh)) {
            int neighbor_idx = halfedge_mesh->to_vertex_handle(heh).idx();
            neighbors.push_back(neighbor_idx);

            double cot_weight = 0.0;

            // 当前面
            auto fh = halfedge_mesh->face_handle(heh);
            if (fh.is_valid()) {
                const auto &tri = triangles[fh.idx()];
                if (tri.v0 != i && tri.v0 != neighbor_idx) cot_weight += tri.cot0;
                else if (tri.v1 != i && tri.v1 != neighbor_idx) cot_weight += tri.cot1;
                else cot_weight += tri.cot2;
            }

            // 对面三角形
            auto heh_opp = halfedge_mesh->opposite_halfedge_handle(heh);
            auto fh_opp = halfedge_mesh->face_handle(heh_opp);
            if (fh_opp.is_valid()) {
                const auto &tri = triangles[fh_opp.idx()];
                if (tri.v0 != i && tri.v0 != neighbor_idx) cot_weight += tri.cot0;
                else if (tri.v1 != i && tri.v1 != neighbor_idx) cot_weight += tri.cot1;
                else cot_weight += tri.cot2;
            }

            cot_weights.push_back(cot_weight);
        }
        
        // 对角线元素
        double diag_sum = 0.0;
        for (size_t k = 0; k < neighbors.size(); ++k) {
            diag_sum += cot_weights[k];
        }
        triplets_global.push_back(Eigen::Triplet<double>(i, i, diag_sum));
        
        // 非对角线元素
        for (size_t k = 0; k < neighbors.size(); ++k) {
            int neighbor_idx = neighbors[k];
            triplets_global.push_back(Eigen::Triplet<double>(i, neighbor_idx, -cot_weights[k]));
        }
    }
    
    A_global.setFromTriplets(triplets_global.begin(), triplets_global.end());
    
    // 添加固定点硬约束（避免平凡解）
    // ARAP只需要1个固定点来消除平移歧义性
    // ASAP需要2个固定点来防止坍缩（允许缩放）
    // 使用惩罚法：给固定点的对角线加极大权重（1e8），保持矩阵对称
    // 注意：由于现在对所有顶点构建系统，固定点位置就是全局顶点索引
    
    if (fixed_idx1 >= 0) {
        // 给固定点的对角线加极大权重
        A_global.coeffRef(fixed_idx1, fixed_idx1) += PENALTY_WEIGHT;
    }
    
    if (fixed_idx2 >= 0) {
        // 给第二个固定点的对角线加极大权重（ASAP模式需要）
        A_global.coeffRef(fixed_idx2, fixed_idx2) += PENALTY_WEIGHT;
    }
    
    // 预分解矩阵（在迭代中复用）
    // 注意：由于使用了惩罚法，矩阵保持对称正定，可以使用Cholesky分解
    Eigen::SimplicialCholesky<Eigen::SparseMatrix<double>> global_solver(A_global);
    
    if (global_solver.info() != Eigen::Success) {
        throw std::runtime_error("Global linear system pre-factorization failed");
    }
    
    std::cout << "[ARAP] Global linear system pre-factorization completed" << std::endl;
    
    // 用于存储前一次迭代的能量值
    double previous_energy = 0.0;
    
    // ASAP是线性问题，不需要迭代；ARAP需要迭代
    if (use_asap && lambda_param == 0.0) {
        // 纯 ASAP 模式（Lambda==0）：直接一次性求解即可
        std::cout << "[ARAP] ASAP is a linear problem, solving directly without iteration" << std::endl;
        max_iterations = 1;  // 强制只执行一次迭代
    } else if (use_asap && lambda_param != 0.0) {
        std::cout << "[ARAP] ASAP requested but lambda!=0 — using hybrid iterative mode" << std::endl;
    }
    
    // ARAP迭代
    for (int iter = 0; iter < max_iterations; ++iter) {
        if (!use_asap) {
            // 迭代开始信息（仅在调试日志启用时）
            if (enable_debug_log) {
                std::cout << "[ARAP] Iteration " << (iter + 1) << "/" << max_iterations << std::endl;
            }
        }
        
        // 如果是纯 ASAP 模式：构建增广线性系统 (u,a,b) 并直接求解（等价于 LSCM/ASAP）
        if (use_asap && lambda_param == 0.0) {
            if (enable_debug_log) {
                std::cout << "[ASAP] Assembling direct linear system (u,a,b)" << std::endl;
            }

            // 系统未知数顺序： [u_x(0..V-1), u_y(0..V-1), a(0..T-1), b(0..T-1)]
            int N = 2 * n_vertices + 2 * n_faces;
            Eigen::SparseMatrix<double> M(N, N);
            std::vector<Eigen::Triplet<double>> triplets_aug;
            Eigen::VectorXd rhs_aug = Eigen::VectorXd::Zero(N);

            // 常量 C^T*C = 2 * I_2
            // 对每个三角形组装局部块矩阵并汇总
            for (int t = 0; t < n_faces; ++t) {
                const auto &tri = triangles[t];

                // 构建局部二维坐标 X（2x2）
                Eigen::Matrix2d X;
                X(0, 0) = tri.x1_local[0] - tri.x0_local[0]; X(1, 0) = tri.x1_local[1] - tri.x0_local[1];
                X(0, 1) = tri.x2_local[0] - tri.x0_local[0]; X(1, 1) = tri.x2_local[1] - tri.x0_local[1];

                double detX = X.determinant();
                double abs_detX = std::abs(detX);
                if (abs_detX < DET_EPS) {
                    // 跳过退化三角形（不会贡献方程）
                    continue;
                }

                Eigen::Matrix2d Y = X.inverse();
                double y00 = Y(0, 0), y01 = Y(0, 1), y10 = Y(1, 0), y11 = Y(1, 1);

                // 构造 B 矩阵的 4x6 系数（J = B * Uvec）
                double B[4][6] = {
                    {-(y00 + y10), 0.0,  y00, 0.0,  y10, 0.0},
                    {-(y01 + y11), 0.0,  y01, 0.0,  y11, 0.0},
                    {0.0, -(y00 + y10), 0.0, y00, 0.0, y10},
                    {0.0, -(y01 + y11), 0.0, y01, 0.0, y11}
                };

                // 计算 BtB = B^T * B（6x6）并乘以面积权重 A
                double BtB[6][6];
                for (int p = 0; p < 6; ++p) for (int q = 0; q < 6; ++q) BtB[p][q] = 0.0;
                for (int p = 0; p < 6; ++p) {
                    for (int q = 0; q < 6; ++q) {
                        double s = 0.0;
                        for (int r = 0; r < 4; ++r) s += B[r][p] * B[r][q];
                        BtB[p][q] = tri.area * s;
                    }
                }

                // 计算 BtC = B^T * C（6x2），其中 C 的行是 [1,0],[0,-1],[0,1],[1,0]
                double BtC[6][2];
                for (int p = 0; p < 6; ++p) {
                    // BtC[p][0] = B[0][p] + B[3][p]
                    // BtC[p][1] = -B[1][p] + B[2][p]
                    BtC[p][0] = B[0][p] + B[3][p];
                    BtC[p][1] = -B[1][p] + B[2][p];
                    // 带面积因子
                    BtC[p][0] *= tri.area;
                    BtC[p][1] *= tri.area;
                }

                // K_aa = A * C^T C = 2A * I2
                double Kaa00 = 2.0 * tri.area;
                double Kaa11 = 2.0 * tri.area;

                // 局部未知量索引映射
                int vi0 = tri.v0;
                int vi1 = tri.v1;
                int vi2 = tri.v2;
                int idxs[6] = { vi0, n_vertices + vi0, vi1, n_vertices + vi1, vi2, n_vertices + vi2 };

                // 汇总 BtB 到全局矩阵
                for (int p = 0; p < 6; ++p) {
                    for (int q = 0; q < 6; ++q) {
                        double val = BtB[p][q];
                        if (std::abs(val) > 0.0) triplets_aug.emplace_back(idxs[p], idxs[q], val);
                    }
                }

                // 汇总 BtC（Top-right 和 Bottom-left）: K_{u,a} = -BtC , K_{a,u} = -BtC^T
                int idx_a = 2 * n_vertices + t;
                int idx_b = 2 * n_vertices + n_faces + t;
                for (int p = 0; p < 6; ++p) {
                    double vala = -BtC[p][0];
                    double valb = -BtC[p][1];
                    if (std::abs(vala) > 0.0) {
                        triplets_aug.emplace_back(idxs[p], idx_a, vala);
                        triplets_aug.emplace_back(idx_a, idxs[p], vala);
                    }
                    if (std::abs(valb) > 0.0) {
                        triplets_aug.emplace_back(idxs[p], idx_b, valb);
                        triplets_aug.emplace_back(idx_b, idxs[p], valb);
                    }
                }

                // 汇总 K_aa
                triplets_aug.emplace_back(idx_a, idx_a, Kaa00);
                triplets_aug.emplace_back(idx_b, idx_b, Kaa11);
            }

            // 固定点惩罚约束（确保解的唯一性）
            if (fixed_idx1 >= 0) {
                int ux = fixed_idx1;
                int uy = n_vertices + fixed_idx1;
                triplets_aug.emplace_back(ux, ux, PENALTY_WEIGHT);
                triplets_aug.emplace_back(uy, uy, PENALTY_WEIGHT);
                rhs_aug(ux) = PENALTY_WEIGHT * uv_coords[fixed_idx1].x;
                rhs_aug(uy) = PENALTY_WEIGHT * uv_coords[fixed_idx1].y;
            }
            if (fixed_idx2 >= 0 && fixed_idx2 != fixed_idx1) {
                int ux = fixed_idx2;
                int uy = n_vertices + fixed_idx2;
                triplets_aug.emplace_back(ux, ux, PENALTY_WEIGHT);
                triplets_aug.emplace_back(uy, uy, PENALTY_WEIGHT);
                rhs_aug(ux) = PENALTY_WEIGHT * uv_coords[fixed_idx2].x;
                rhs_aug(uy) = PENALTY_WEIGHT * uv_coords[fixed_idx2].y;
            }

            // 构建稀疏矩阵并求解（使用 SparseLU 以便对称或非对称情况更健壮）
            M.setFromTriplets(triplets_aug.begin(), triplets_aug.end());
            Eigen::SparseLU<Eigen::SparseMatrix<double>> asap_solver;
            asap_solver.analyzePattern(M);
            asap_solver.factorize(M);
            if (asap_solver.info() != Eigen::Success) {
                std::cerr << "[ASAP ERROR] Factorization failed" << std::endl;
                break;
            }

            Eigen::VectorXd x = asap_solver.solve(rhs_aug);
            if (asap_solver.info() != Eigen::Success || !x.allFinite()) {
                std::cerr << "[ASAP ERROR] Solve failed or produced non-finite values" << std::endl;
                break;
            }

            // 更新 UV
            for (int vi = 0; vi < n_vertices; ++vi) {
                uv_coords[vi].x = static_cast<float>(x(vi));
                uv_coords[vi].y = static_cast<float>(x(n_vertices + vi));
            }

            // 计算 ASAP 能量（复用 J 计算与 SVD 求取最优相似矩阵以评估能量）
            double asap_energy = 0.0;
            for (int t = 0; t < n_faces; ++t) {
                const auto &tri = triangles[t];
                // 跳过退化三角形
                Eigen::Matrix2d X;
                X(0, 0) = tri.x1_local[0] - tri.x0_local[0]; X(1, 0) = tri.x1_local[1] - tri.x0_local[1];
                X(0, 1) = tri.x2_local[0] - tri.x0_local[0]; X(1, 1) = tri.x2_local[1] - tri.x0_local[1];
                double detX = X.determinant();
                if (std::abs(detX) < DET_EPS) continue;

                // 计算 J = U * X^{-1}
                Eigen::Matrix2d Umat;
                Umat(0, 0) = uv_coords[tri.v1].x - uv_coords[tri.v0].x;
                Umat(1, 0) = uv_coords[tri.v1].y - uv_coords[tri.v0].y;
                Umat(0, 1) = uv_coords[tri.v2].x - uv_coords[tri.v0].x;
                Umat(1, 1) = uv_coords[tri.v2].y - uv_coords[tri.v0].y;
                Eigen::Matrix2d J = Umat * X.inverse();

                // 最优相似变换 L = U_svd * diag(s,s) * V^T
                Eigen::JacobiSVD<Eigen::Matrix2d> svd(J, Eigen::ComputeFullU | Eigen::ComputeFullV);
                Eigen::Matrix2d U_svd = svd.matrixU();
                Eigen::Matrix2d V_svd = svd.matrixV();
                Eigen::Vector2d sv = svd.singularValues();
                if ((U_svd * V_svd.transpose()).determinant() < 0) V_svd.col(1) *= -1;
                double s = 0.5 * (sv[0] + sv[1]);
                Eigen::DiagonalMatrix<double, 2, 2> Sigma_avg(s, s);
                Eigen::Matrix2d L = U_svd * Sigma_avg * V_svd.transpose();

                if (!J.allFinite() || !L.allFinite()) continue;
                Eigen::Matrix2d D = J - L;
                double fn = D.squaredNorm();
                if (!std::isfinite(fn)) continue;
                asap_energy += tri.area * fn;
            }

            if (enable_debug_log) {
                std::cout << "[ASAP] Direct solve completed, energy: " << asap_energy << std::endl;
            }
            // ASAP 只需一次求解，跳出迭代
            break;
        }

        /* ==================== 局部阶段 ====================
        ** 对每个三角形，计算最优变换矩阵L_t
        ** ARAP：纯旋转矩阵（奇异值置1）
        ** ASAP：相似矩阵（奇异值取均值）
        */
        
        std::vector<Eigen::Matrix2d> L_matrices(n_faces);

        // 局部阶段：统计信息，用于调试能量为0的问题
        int local_L_identity_count = 0;
        int local_J_invalid_count = 0;
        double local_min_abs_det = std::numeric_limits<double>::infinity();
        double local_max_abs_det = 0.0;
        
        // 【OpenMP验证】检查OpenMP是否可用
        #ifdef _OPENMP
        if (enable_parallel) {
            std::cout << "[ARAP] OpenMP is enabled, max threads: " << omp_get_max_threads() << std::endl;
        } else {
            std::cout << "[ARAP] OpenMP is available but disabled by user" << std::endl;
        }
        #else
        std::cout << "[ARAP] OpenMP is NOT available (serial execution)" << std::endl;
        #endif
        
        // 【核心并行语句】根据enable_parallel参数决定是否使用并行计算
        #pragma omp parallel for default(none) shared(n_faces, triangles, uv_coords, L_matrices, use_asap, lambda_param, DET_EPS, enable_parallel, enable_debug_log) schedule(static) if(enable_parallel)

        // Local phase: Compute optimal rotation matrix L_t for each triangle
        // Using Jacobian J = [u1-u0, u2-u0] * [x1-x0, x2-x0]^{-1}
        // Then perform SVD on J to get optimal rotation L = U_svd * V_svd^T
        for (int t = 0; t < n_faces; ++t) {
            const auto& tri = triangles[t];
            
            // 【OpenMP验证】打印线程号和三角形索引，验证多核并行（仅在并行计算和调试日志都启用时）
            #ifdef _OPENMP
            if (enable_parallel && enable_debug_log) {
                printf("Thread %d, triangle %d\n", omp_get_thread_num(), t);
            }
            #endif

            bool used_identity = false;

            // Build U matrix from current UV coordinates
            Eigen::Matrix2d U;
            U(0, 0) = uv_coords[tri.v1].x - uv_coords[tri.v0].x;
            U(1, 0) = uv_coords[tri.v1].y - uv_coords[tri.v0].y;
            U(0, 1) = uv_coords[tri.v2].x - uv_coords[tri.v0].x;
            U(1, 1) = uv_coords[tri.v2].y - uv_coords[tri.v0].y;

            // Check for NaN in UV coordinates
            if (!U.allFinite()) {
                std::cerr << "[ARAP ERROR] NaN detected in UV coordinates for triangle " << t << std::endl;
                L_matrices[t] = Eigen::Matrix2d::Identity();
                local_L_identity_count++;
                continue;
            }

            // Build X matrix (local triangle coordinates)
            Eigen::Matrix2d X;
            X(0, 0) = tri.x1_local[0] - tri.x0_local[0]; X(1, 0) = tri.x1_local[1] - tri.x0_local[1];
            X(0, 1) = tri.x2_local[0] - tri.x0_local[0]; X(1, 1) = tri.x2_local[1] - tri.x0_local[1];

            // Check for degenerate triangle (using absolute value and relaxed threshold)
            double detX = X.determinant();
            double abs_detX = std::abs(detX);
            local_min_abs_det = std::min(local_min_abs_det, abs_detX);
            local_max_abs_det = std::max(local_max_abs_det, abs_detX);
            if (abs_detX < DET_EPS) {
                // Degenerate triangle, use identity matrix
                L_matrices[t] = Eigen::Matrix2d::Identity();
                used_identity = true;
                local_L_identity_count++;
                continue;
            }

            // Compute Jacobian J = U * X^{-1}
            Eigen::Matrix2d J = U * X.inverse();

            // Check for NaN/Inf in Jacobian
            if (!J.allFinite()) {
                std::cerr << "[ARAP ERROR] NaN/Inf detected in Jacobian for triangle " << t << std::endl;
                L_matrices[t] = Eigen::Matrix2d::Identity();
                used_identity = true;
                local_J_invalid_count++;
                continue;
            }

            // Perform SVD on J to get optimal rotation L = U_svd * V_svd^T
            Eigen::JacobiSVD<Eigen::Matrix2d> svd(J, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::Matrix2d U_svd = svd.matrixU();
            Eigen::Matrix2d V_svd = svd.matrixV();
            Eigen::Vector2d singular_values = svd.singularValues();

            // Ensure pure rotation (avoid reflection)
            if ((U_svd * V_svd.transpose()).determinant() < 0) {
                V_svd.col(1) *= -1;
            }

            Eigen::Matrix2d L;
            // Hybrid scheme controlled by lambda_param:
            // lambda_param == 0 -> pure ASAP (similarity with avg scale)
            // lambda_param -> +inf -> pure ARAP (rigid rotation)
            if (lambda_param > 0.0) {
                double avg = 0.5 * (singular_values[0] + singular_values[1]);
                double s = (avg + lambda_param) / (1.0 + lambda_param);
                constexpr double MIN_SCALE = 1e-8;
                s = std::max(s, MIN_SCALE);
                Eigen::DiagonalMatrix<double, 2, 2> Sigma_h(s, s);
                L = U_svd * Sigma_h * V_svd.transpose();
            } else if (use_asap) {
                double s = 0.5 * (singular_values[0] + singular_values[1]);
                constexpr double MIN_SCALE = 1e-8;
                s = std::max(s, MIN_SCALE);
                Eigen::DiagonalMatrix<double, 2, 2> Sigma_avg(s, s);
                L = U_svd * Sigma_avg * V_svd.transpose();
            } else {
                // pure ARAP rotation
                L = U_svd * V_svd.transpose();
            }

            // Check for NaN/Inf in rotation matrix
            if (!L.allFinite()) {
                std::cerr << "[ARAP ERROR] NaN/Inf detected in rotation matrix for triangle " << t << std::endl;
                L = Eigen::Matrix2d::Identity();
                used_identity = true;
                local_J_invalid_count++;
            }

            if (used_identity) local_L_identity_count++;
            L_matrices[t] = L;
        }
        
        // 【调试日志】局部阶段统计信息（仅在调试日志启用时）
        if (enable_debug_log) {
            std::cout << "[ARAP][Debug] Local phase: L_identity=" << local_L_identity_count
                      << ", J_invalid=" << local_J_invalid_count
                      << ", detX_min=" << local_min_abs_det << ", detX_max=" << local_max_abs_det << std::endl;
        }
        
        /* ==================== Global Phase ====================
        ** Fix L_t, update UV coordinates
        ** Solve linear system: A * u = b
        ** According to paper: sum_{(i,j)∈E} cot(θ_ij) * [(u_i - u_j) - L_t * (x_i - x_j)] = 0
        ** where x_i - x_j is the 3D edge vector projected to triangle's local 2D basis
        ** Note: ARAP is free-boundary parameterization, all vertices (including boundary) should be optimized
        ** ASAP is linear, ARAP requires iteration
        */
        
        Eigen::VectorXd b_u_global(n_vertices);
        Eigen::VectorXd b_v_global(n_vertices);
        b_u_global.setZero();
        b_v_global.setZero();
        
        for (int i = 0; i < n_vertices; ++i) {
            auto vh = halfedge_mesh->vertex_handle(i);
            
            // 获取当前顶点的3D坐标
            auto p_curr = halfedge_mesh->point(vh);
            
            // 获取邻居顶点和对应的L矩阵
            for (auto heh : halfedge_mesh->voh_range(vh)) {
                int neighbor_idx = halfedge_mesh->to_vertex_handle(heh).idx();
                
                // 获取邻居顶点的3D坐标
                auto vh_neighbor = halfedge_mesh->vertex_handle(neighbor_idx);
                auto p_neighbor = halfedge_mesh->point(vh_neighbor);
                
                // 计算3D边向量（注意方向：x_i - x_j，对齐论文公式）
                OpenMesh::Vec3f edge_3d = p_curr - p_neighbor;
                
                // 当前三角形
                auto fh = halfedge_mesh->face_handle(heh);
                if (fh.is_valid()) {
                    int face_idx = fh.idx();
                    const auto& tri = triangles[face_idx];
                    double c = 0.0;
                    if (tri.v0 != i && tri.v0 != neighbor_idx) c = tri.cot0;
                    else if (tri.v1 != i && tri.v1 != neighbor_idx) c = tri.cot1;
                    else c = tri.cot2;
                    
                    // 添加 L_t * (x_i - x_j) 到右端项
                    if (c > EPSILON) {
                        Eigen::Vector2d dx;
                        dx[0] = edge_3d.dot(tri.basis_x);
                        dx[1] = edge_3d.dot(tri.basis_y);
                        Eigen::Vector2d L_dx = L_matrices[face_idx] * dx;
                        b_u_global(i) += c * L_dx[0];
                        b_v_global(i) += c * L_dx[1];
                    }
                }
                
                // 对面三角形：注意不要重复累加！
                // 每条边(i,j)只应该贡献一次，不应该当前面和对面都加
                // 正确做法：只累加当前面，对面的贡献会在遍历到边(j,i)时自动处理
                auto heh_opp = halfedge_mesh->opposite_halfedge_handle(heh);
                auto fh_opp = halfedge_mesh->face_handle(heh_opp);
                if (fh_opp.is_valid()) {
                    int face_idx = fh_opp.idx();
                    const auto& tri = triangles[face_idx];
                    double c = 0.0;
                    if (tri.v0 != i && tri.v0 != neighbor_idx) c = tri.cot0;
                    else if (tri.v1 != i && tri.v1 != neighbor_idx) c = tri.cot1;
                    else c = tri.cot2;
                    
                    if (c > EPSILON) {
                        Eigen::Vector2d dx;
                        dx[0] = edge_3d.dot(tri.basis_x);
                        dx[1] = edge_3d.dot(tri.basis_y);
                        Eigen::Vector2d L_dx = L_matrices[face_idx] * dx;
                        b_u_global(i) += c * L_dx[0];
                        b_v_global(i) += c * L_dx[1];
                    }
                }
            }
        }
        
        // 设置固定点的右端项约束（使用惩罚法）
        // 惩罚法：b_i = penalty * fixed_value
        if (fixed_idx1 >= 0) {
            b_u_global(fixed_idx1) = PENALTY_WEIGHT * uv_coords[fixed_idx1].x;
            b_v_global(fixed_idx1) = PENALTY_WEIGHT * uv_coords[fixed_idx1].y;
        }
        if (fixed_idx2 >= 0) {
            b_u_global(fixed_idx2) = PENALTY_WEIGHT * uv_coords[fixed_idx2].x;
            b_v_global(fixed_idx2) = PENALTY_WEIGHT * uv_coords[fixed_idx2].y;
        }
        
        // Solve linear system
        Eigen::VectorXd u_new = global_solver.solve(b_u_global);
        Eigen::VectorXd v_new = global_solver.solve(b_v_global);
        
        if (global_solver.info() != Eigen::Success) {
            std::cerr << "[ARAP ERROR] Iteration " << (iter + 1) << " solver failed" << std::endl;
            break;
        }
        
        // Check for NaN in solution
        bool solution_has_nan = false;
        for (int i = 0; i < n_vertices; ++i) {
            if (std::isnan(u_new(i)) || std::isnan(v_new(i))) {
                std::cerr << "[ARAP ERROR] NaN detected in solution for vertex " << i << std::endl;
                solution_has_nan = true;
            }
        }
        if (solution_has_nan) {
            std::cerr << "[ARAP ERROR] Solution contains NaN, stopping iteration" << std::endl;
            break;
        }
        
        // Update UV coordinates for all vertices (including boundary and interior)
        // After using penalty method, solver automatically maintains fixed point constraints
        // But we still skip fixed points to ensure constraints are strictly satisfied
        for (int i = 0; i < n_vertices; ++i) {
            if (i == fixed_idx1 || i == fixed_idx2) {
                // Skip fixed points, keep their UV coordinates unchanged
                continue;
            }
            uv_coords[i] = glm::vec2(u_new(i), v_new(i));
        }
        
        // Compute true ARAP energy: E(u, L) = sum_t A_t * ||J_t(u) - L_t||_F^2
        // Using 2x2 Jacobian matrix according to paper
        double current_energy = 0.0;

        // Energy debug counters
        int energy_skipped_degX = 0;
        int energy_skipped_invalidJ = 0;
        int energy_skipped_nonfinite = 0;
        int energy_contrib_count = 0;
        double energy_min_abs_det = std::numeric_limits<double>::infinity();
        double energy_max_abs_det = 0.0;

        for (int t = 0; t < n_faces; ++t) {
            const auto& tri = triangles[t];
            const auto& L_t = L_matrices[t];
            
            // 获取3D顶点坐标
            auto vh0 = halfedge_mesh->vertex_handle(tri.v0);
            auto vh1 = halfedge_mesh->vertex_handle(tri.v1);
            auto vh2 = halfedge_mesh->vertex_handle(tri.v2);
            OpenMesh::Vec3f p0 = halfedge_mesh->point(vh0);
            OpenMesh::Vec3f p1 = halfedge_mesh->point(vh1);
            OpenMesh::Vec3f p2 = halfedge_mesh->point(vh2);
            
            // 计算雅可比矩阵J_t（2×2）
            // 根据论文：J = [u1-u0, u2-u0] * [x1-x0, x2-x0]^(-1)
            // 其中u_i是2D参数坐标，x_i是3D顶点在局部坐标系中的坐标
            
            // 构建2D边向量矩阵（2×2）
            Eigen::Matrix2d U;
            U(0, 0) = uv_coords[tri.v1].x - uv_coords[tri.v0].x;
            U(1, 0) = uv_coords[tri.v1].y - uv_coords[tri.v0].y;
            U(0, 1) = uv_coords[tri.v2].x - uv_coords[tri.v0].x;
            U(1, 1) = uv_coords[tri.v2].y - uv_coords[tri.v0].y;
            
            // 构建2D边向量矩阵（2×2，使用局部坐标系投影）
            // 将3D边向量投影到三角形局部2D坐标系
            Eigen::Matrix2d X;
            OpenMesh::Vec3f edge1_3d = p1 - p0;
            OpenMesh::Vec3f edge2_3d = p2 - p0;
            X(0, 0) = edge1_3d.dot(tri.basis_x); X(1, 0) = edge1_3d.dot(tri.basis_y);
            X(0, 1) = edge2_3d.dot(tri.basis_x); X(1, 1) = edge2_3d.dot(tri.basis_y);
            
            double detX = X.determinant();
            double abs_detX = std::abs(detX);
            energy_min_abs_det = std::min(energy_min_abs_det, abs_detX);
            energy_max_abs_det = std::max(energy_max_abs_det, abs_detX);
            if (abs_detX < DET_EPS) {
                energy_skipped_degX++;
                continue;  // 跳过退化的三角形
            }
            
            // 计算雅可比矩阵：J = U * X^(-1)
            Eigen::Matrix2d J = U * X.inverse();
            
            // Check if J matrix contains NaN or Inf
            if (!J.allFinite()) {
                std::cerr << "[ARAP ERROR] NaN/Inf in Jacobian during energy computation for triangle " << t << std::endl;
                energy_skipped_invalidJ++;
                continue;  // Skip invalid triangle
            }
            
            // Compute Frobenius norm squared of J_t - L_t
            Eigen::Matrix2d J_minus_L = J - L_t;
            double frobenius_norm_sq = J_minus_L.squaredNorm();
            
            // Check if energy is NaN or Inf
            if (!std::isfinite(frobenius_norm_sq)) {
                std::cerr << "[ARAP ERROR] NaN/Inf in energy for triangle " << t << std::endl;
                energy_skipped_nonfinite++;
                continue;  // Skip invalid triangle
            }

            current_energy += tri.area * frobenius_norm_sq;
            energy_contrib_count++;
        }
        
        // 【调试日志】能量计算统计信息（仅在调试日志启用时）
        if (enable_debug_log) {
            std::cout << "[ARAP][Debug] Energy computation: contrib=" << energy_contrib_count
                      << ", skipped_degX=" << energy_skipped_degX
                      << ", skipped_invalidJ=" << energy_skipped_invalidJ
                      << ", skipped_nonfinite=" << energy_skipped_nonfinite
                      << ", det_min=" << energy_min_abs_det << ", det_max=" << energy_max_abs_det
                      << std::endl;
        }
        
        // 计算能量变化（相对变化）
        double energy_change = 0.0;
        if (iter == 0) {
            // 第一次迭代：记录初始能量
            previous_energy = current_energy;
            energy_change = current_energy;  // 绝对能量
        } else {
            // 后续迭代：计算相对变化（仅ARAP需要）
            if (!use_asap && previous_energy > EPSILON) {
                energy_change = std::abs(current_energy - previous_energy) / previous_energy;
            } else if (!use_asap) {
                energy_change = std::abs(current_energy - previous_energy);
            }
            // ASAP模式不需要能量变化判断（只执行一次）
        }
        
        previous_energy = current_energy;
        
        if (!use_asap) {
            // ARAP模式：输出迭代信息（仅在调试日志启用时）
            if (enable_debug_log) {
                std::cout << "[ARAP] Iteration " << (iter + 1) << " completed, energy: " << current_energy 
                          << ", energy change: " << energy_change << std::endl;
            }
            
            // Early termination if energy change is very small (relative change < 1e-6)
            if (iter > 0 && energy_change < 1e-6) {
                std::cout << "[ARAP] Converged after " << (iter + 1) << " iterations, early termination" << std::endl;
                break;
            }
        } else {
            // ASAP模式：输出最终能量（只执行一次）
            std::cout << "[ASAP] Single iteration completed, energy: " << current_energy << std::endl;
        }
    }
    
    std::cout << "[ARAP] Parameterization completed" << std::endl;
    
    // Normalize UV coordinates to [0, 1] range if requested
    if (normalize_uv) {
        std::cout << "[ARAP] Normalizing UV coordinates to [0, 1] range" << std::endl;
        
        // Find min and max of UV coordinates
        float min_u = std::numeric_limits<float>::max();
        float max_u = -std::numeric_limits<float>::max();
        float min_v = std::numeric_limits<float>::max();
        float max_v = -std::numeric_limits<float>::max();
        
        for (const auto& uv : uv_coords) {
            min_u = std::min(min_u, uv.x);
            max_u = std::max(max_u, uv.x);
            min_v = std::min(min_v, uv.y);
            max_v = std::max(max_v, uv.y);
        }
        
        std::cout << "[ARAP] Original UV range: u=[" << min_u << ", " << max_u << "], v=[" << min_v << ", " << max_v << "]" << std::endl;
        
        // Compute scale factors
        float range_u = max_u - min_u;
        float range_v = max_v - min_v;
        float scale_u = (range_u > EPSILON) ? 1.0f / range_u : 1.0f;
        float scale_v = (range_v > EPSILON) ? 1.0f / range_v : 1.0f;
        
        // Normalize UV coordinates
        for (int i = 0; i < n_vertices; ++i) {
            uv_coords[i].x = (uv_coords[i].x - min_u) * scale_u;
            uv_coords[i].y = (uv_coords[i].y - min_v) * scale_v;
        }
        
        std::cout << "[ARAP] Normalized UV range: [0, 1]" << std::endl;
    } else {
        std::cout << "[ARAP] Skipping UV normalization" << std::endl;
    }
    
    // Output results
    params.set_output("OutputUV", uv_coords);
    
    // Apply UV coordinates to original 3D mesh vertices (z=0)
    // Create a copy of output geometry
    auto output_geometry = input;
    
    // Get mesh component
    auto output_mesh = output_geometry.get_component<MeshComponent>();
    if (output_mesh) {
        // Get current vertices
        auto vertices = output_mesh->get_vertices();
        
        // Apply UV coordinates to vertices (x=uv.x, y=uv.y, z=0)
        for (int i = 0; i < n_vertices; ++i) {
            vertices[i].x = uv_coords[i].x;
            vertices[i].y = uv_coords[i].y;
            vertices[i].z = 0.0f;  // z coordinate set to 0
        }
        
        // Set modified vertices
        output_mesh->set_vertices(vertices);
    }
    
    params.set_output("OutputMesh", std::move(output_geometry));
}

NODE_DECLARATION_UI(hw6_arap);
NODE_DEF_CLOSE_SCOPE