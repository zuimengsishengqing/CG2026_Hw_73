// utils/min_surf_harmo.h
#pragma once
#include "tutte_parameterizer.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class MinSurfHarmo : public TutteParameterizer {
public:
    explicit MinSurfHarmo(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original = nullptr);
    void execute() override;

private:
    double compute_harmo_weight(int vi, int vj);
};

USTC_CG_NAMESPACE_CLOSE_SCOPE