// utils/square_mapping.cpp
#include "square_mapping.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

SquareMapping::SquareMapping(std::shared_ptr<PolyMesh> mesh) : TutteParameterizer(mesh) {}

void SquareMapping::execute() {
    int boundary_count = boundary_indices_.size();
    if (boundary_count < 4) {
        throw std::runtime_error("SquareMapping: Too few boundary points.");
    }

    int side_boundary = boundary_count / 4;
    int remainder = boundary_count % 4;

    for (int i = 0; i < boundary_count; ++i) {
        float x, y, z = 0.0f;
        float t = static_cast<float>(i) / boundary_count; // 参数化位置
        /*if (i < side_boundary + (remainder > 0)) { // Bottom side
            x = -1.0f + 2.0f * static_cast<float>(i) / (side_boundary + (remainder > 0));
            y = -1.0f;
        } else if (i < 2 * side_boundary + (remainder > 1)) { // Right side
            x = 1.0f;
            y = -1.0f + 2.0f * static_cast<float>(i - (side_boundary + 
                (remainder > 0))) / (side_boundary + (remainder > 1));
        } else if (i < 3 * side_boundary + (remainder > 2)) { // Top side
            x = 1.0f - 2.0f * static_cast<float>(i - (2 * side_boundary +
                (remainder > 1))) / (side_boundary + (remainder > 2));
            y = 1.0f;
        } else { // Left side
            x = -1.0f;
            y = 1.0f - 2.0f * static_cast<float>(i - (3 * side_boundary + 
                (remainder > 2))) / (side_boundary + (remainder > 3));
        }*/
        if (i < side_boundary + (remainder > 0)) { // Bottom side
            x = 0.5 * (2.0f * static_cast<float>(i) / (side_boundary + (remainder > 0)));
            y = 0;
        } else if (i < 2 * side_boundary + (remainder > 1)) { // Right side
            x = 0.5 * 2.0f;
            y = 0.5 * (2.0f * static_cast<float>(i - (side_boundary + 
                (remainder > 0))) / (side_boundary + (remainder > 1)));
        } else if (i < 3 * side_boundary + (remainder > 2)) { // Top side
            x = 0.5 * (2.0f - 2.0f * static_cast<float>(i - (2 * side_boundary +
                (remainder > 1))) / (side_boundary + (remainder > 2)));
            y = 0.5 * 2.0f;
        } else { // Left side
            x = 0;
            y = 0.5 * (2.0f - 2.0f * static_cast<float>(i - (3 * side_boundary + 
                (remainder > 2))) / (side_boundary + (remainder > 3)));
        }
        int vertex_idx = boundary_indices_[i];
        mesh_->point(mesh_->vertex_handle(vertex_idx)) = OpenMesh::Vec3f(x, y, z);
    }
}
USTC_CG_NAMESPACE_CLOSE_SCOPE