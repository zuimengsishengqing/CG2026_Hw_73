#include <Eigen/Sparse>

#include "GCore/Components/MeshComponent.h"
#include "GCore/util_openmesh_bind.h"
#include "geom_node_base.h"
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>

/*
** @brief HW4_TutteParameterization
**
** This file contains two nodes whose primary function is to map the boundary of
*a mesh to a plain
** convex closed curve (circle of square), setting the stage for subsequent
*Laplacian equation
** solution and mesh parameterization tasks.
**
** Key to this node's implementation is the adept manipulation of half-edge data
*structures
** to identify and modify the boundary of the mesh.
**
** Task Overview:
** - The two execution functions (node_map_boundary_to_square_exec,
** node_map_boundary_to_circle_exec) require an update to accurately map the
*mesh boundary to a and
** circles. This entails identifying the boundary edges, evenly distributing
*boundary vertices along
** the square's perimeter, and ensuring the internal vertices' positions remain
*unchanged.
** - A focus on half-edge data structures to efficiently traverse and modify
*mesh boundaries.
*/

NODE_DEF_OPEN_SCOPE

/*
** HW4_TODO: Node to map the mesh boundary to a circle.
*/

NODE_DECLARATION_FUNCTION(hw5_circle_boundary_mapping)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");
    // Output-1: Processed 3D mesh whose boundary is mapped to a square and the
    // interior vertices remains the same
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_circle_boundary_mapping)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");

    // (TO BE UPDATED) Avoid processing the node when there is no input
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Boundary Mapping: Need Geometry Input.");
    }
    // Build halfedge mesh
    auto halfedge_mesh = operand_to_openmesh(&input);

    // Robust boundary extraction using map (handles non-contiguous indices)
    std::unordered_map<int,int> boundary_next;
    for (const auto& heh : halfedge_mesh->halfedges()) {
        if (heh.is_boundary()) {
            boundary_next[heh.from().idx()] = heh.to().idx();
        }
    }

    // If no boundary, return original geometry
    if (boundary_next.empty()) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    // Collect boundary loops (may be multiple); pick the longest
    std::vector<std::vector<int>> loops;
    std::unordered_set<int> visited;
    for (const auto& kv : boundary_next) {
        int start = kv.first;
        if (visited.count(start)) continue;
        std::vector<int> loop;
        int cur = start;
        loop.push_back(cur);
        visited.insert(cur);
        // follow next pointers until we close the loop or break
        while (true) {
            auto it = boundary_next.find(cur);
            if (it == boundary_next.end()) break;
            cur = it->second;
            if (cur == start) break;
            if (visited.count(cur)) break;
            loop.push_back(cur);
            visited.insert(cur);
        }
        if (!loop.empty()) {
            // close the loop by repeating the first index at the end (consistent with other nodes)
            if (loop.front() != loop.back()) loop.push_back(loop.front());
            loops.push_back(std::move(loop));
        }
    }

    if (loops.empty()) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    // choose the longest loop
    auto it_long = std::max_element(loops.begin(), loops.end(), [](const auto &a, const auto &b){ return a.size() < b.size(); });
    std::vector<int> boundary = *it_long;

    // number of unique boundary vertices (exclude duplicate last)
    int n = (boundary.front() == boundary.back() && boundary.size() > 1) ? int(boundary.size()) - 1 : int(boundary.size());
    if (n <= 0) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    // compute segment lengths along boundary
    std::vector<double> seg_len(n);
    double total_len = 0.0;
    for (int i = 0; i < n; ++i) {
        int idx0 = boundary[i];
        int idx1 = boundary[(i + 1) % n];
        auto p0 = halfedge_mesh->point(halfedge_mesh->vertex_handle(idx0));
        auto p1 = halfedge_mesh->point(halfedge_mesh->vertex_handle(idx1));
        double l = (p1 - p0).length();
        seg_len[i] = l;
        total_len += l;
    }

    // build cumulative lengths
    std::vector<double> cum(n);
    cum[0] = 0.0;
    for (int i = 1; i < n; ++i) cum[i] = cum[i-1] + seg_len[i-1];

    // map boundary vertices to unit circle inside [0,1]^2 (center (0.5,0.5), radius 0.5)
    const double TWO_PI = 2.0 * M_PI;
    for (int i = 0; i < n; ++i) {
        double s = (total_len > 0.0) ? (cum[i] / total_len) : (double(i) / double(n));
        double theta = TWO_PI * s;
        double u = 0.5 + 0.5 * std::cos(theta);
        double v = 0.5 + 0.5 * std::sin(theta);
        int vid = boundary[i];
        auto vh = halfedge_mesh->vertex_handle(vid);
        halfedge_mesh->point(vh)[0] = u;
        halfedge_mesh->point(vh)[1] = v;
        halfedge_mesh->point(vh)[2] = 0.0;
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

/*
** HW4_TODO: Node to map the mesh boundary to a square.
*/

NODE_DECLARATION_FUNCTION(hw5_square_boundary_mapping)
{
    // Input-1: Original 3D mesh with boundary
    b.add_input<Geometry>("Input");

    // Output-1: Processed 3D mesh whose boundary is mapped to a square and the
    // interior vertices remains the same
    b.add_output<Geometry>("Output");
}

NODE_EXECUTION_FUNCTION(hw5_square_boundary_mapping)
{
    // Get the input from params
    auto input = params.get_input<Geometry>("Input");

    // (TO BE UPDATED) Avoid processing the node when there is no input
    if (!input.get_component<MeshComponent>()) {
        throw std::runtime_error("Input does not contain a mesh");
    }
    // Build halfedge mesh
    auto halfedge_mesh = operand_to_openmesh(&input);

    // Robust boundary extraction (like circle mapping)
    std::unordered_map<int,int> boundary_next;
    for (const auto& heh : halfedge_mesh->halfedges()) {
        if (heh.is_boundary()) boundary_next[heh.from().idx()] = heh.to().idx();
    }

    if (boundary_next.empty()) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    std::vector<std::vector<int>> loops;
    std::unordered_set<int> visited;
    for (const auto& kv : boundary_next) {
        int start = kv.first;
        if (visited.count(start)) continue;
        std::vector<int> loop;
        int cur = start;
        loop.push_back(cur);
        visited.insert(cur);
        while (true) {
            auto it = boundary_next.find(cur);
            if (it == boundary_next.end()) break;
            cur = it->second;
            if (cur == start) break;
            if (visited.count(cur)) break;
            loop.push_back(cur);
            visited.insert(cur);
        }
        if (!loop.empty()) {
            if (loop.front() != loop.back()) loop.push_back(loop.front());
            loops.push_back(std::move(loop));
        }
    }

    if (loops.empty()) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    auto it_long = std::max_element(loops.begin(), loops.end(), [](const auto &a, const auto &b){ return a.size() < b.size(); });
    std::vector<int> boundary = *it_long;
    int n = (boundary.front() == boundary.back() && boundary.size() > 1) ? int(boundary.size()) - 1 : int(boundary.size());
    if (n <= 0) {
        auto geometry = openmesh_to_operand(halfedge_mesh.get());
        params.set_output("Output", std::move(*geometry));
        return true;
    }

    // compute lengths
    std::vector<double> seg_len(n);
    double total_len = 0.0;
    for (int i = 0; i < n; ++i) {
        auto p0 = halfedge_mesh->point(halfedge_mesh->vertex_handle(boundary[i]));
        auto p1 = halfedge_mesh->point(halfedge_mesh->vertex_handle(boundary[(i + 1) % n]));
        double l = (p1 - p0).length();
        seg_len[i] = l; total_len += l;
    }
    std::vector<double> cum(n); cum[0] = 0.0; for (int i = 1; i < n; ++i) cum[i] = cum[i-1] + seg_len[i-1];

    // Map to unit square [0,1]^2 by arc-length along perimeter
    // Perimeter parameter t in [0,1) mapped to 4 edges
    for (int i = 0; i < n; ++i) {
        double s = (total_len > 0.0) ? (cum[i] / total_len) : (double(i) / double(n));
        double tt = s * 4.0; // which edge
        double u = 0.0, v = 0.0;
        if (tt < 1.0) { u = tt; v = 0.0; }
        else if (tt < 2.0) { u = 1.0; v = tt - 1.0; }
        else if (tt < 3.0) { u = 3.0 - tt; v = 1.0; }
        else { u = 0.0; v = 4.0 - tt; }
        int vid = boundary[i];
        auto vh = halfedge_mesh->vertex_handle(vid);
        halfedge_mesh->point(vh)[0] = u;
        halfedge_mesh->point(vh)[1] = v;
        halfedge_mesh->point(vh)[2] = 0.0;
    }

    auto geometry = openmesh_to_operand(halfedge_mesh.get());
    params.set_output("Output", std::move(*geometry));
    return true;
}

NODE_DECLARATION_UI(boundary_mapping);
NODE_DEF_CLOSE_SCOPE