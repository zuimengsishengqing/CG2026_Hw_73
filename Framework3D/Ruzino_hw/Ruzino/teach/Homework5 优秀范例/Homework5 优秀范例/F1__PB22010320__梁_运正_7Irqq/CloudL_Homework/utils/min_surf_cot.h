// utils/min_surf_cot.h
#pragma once
#include "tutte_parameterizer.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class MinSurfCot : public TutteParameterizer {
public:
    explicit MinSurfCot(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original = nullptr);
    void execute() override;

private:
    double compute_cotangent(const OpenMesh::Vec3f& p0, const OpenMesh::Vec3f& p1, const OpenMesh::Vec3f& p2);
};

USTC_CG_NAMESPACE_CLOSE_SCOPE