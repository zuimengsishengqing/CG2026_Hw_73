// utils/min_surf_floater.h
#pragma once
#include "tutte_parameterizer.h"
#include <vector>

USTC_CG_NAMESPACE_OPEN_SCOPE

class MinSurfFloater : public TutteParameterizer {
public:
    explicit MinSurfFloater(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original = nullptr);
    void execute() override;

private:
    double compute_shape_preserving_weight(int vi, int vj);
    void compute_local_params(int vi, std::vector<int>& indices, std::vector<float>& angles, std::vector<float>& distances);
};

USTC_CG_NAMESPACE_CLOSE_SCOPE