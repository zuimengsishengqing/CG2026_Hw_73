// utils/circle_mapping.cpp
#include "circle_mapping.h"
#define M_PI 3.14159265358979323846

USTC_CG_NAMESPACE_OPEN_SCOPE

CircleMapping::CircleMapping(std::shared_ptr<PolyMesh> mesh) : TutteParameterizer(mesh) {}

void CircleMapping::execute() {
    int boundary_count = boundary_indices_.size();
    double angle_step = 2.0 * M_PI / boundary_count;

    for (int i = 0; i < boundary_count; ++i) {
        double theta = i * angle_step;
        float x = 0.5 + 0.5 * std::cos(theta);
        float y = 0.5 + 0.5 * std::sin(theta);
        float z = 0.0f;
        int idx = boundary_indices_[i];
        mesh_->point(mesh_->vertex_handle(idx)) = OpenMesh::Vec3f(x, y, z);
    }
}
USTC_CG_NAMESPACE_CLOSE_SCOPE