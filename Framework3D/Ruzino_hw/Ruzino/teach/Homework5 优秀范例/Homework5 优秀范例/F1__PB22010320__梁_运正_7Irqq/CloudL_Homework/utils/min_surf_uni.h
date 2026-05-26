// utils/min_surf_uni.h
#pragma once
#include "tutte_parameterizer.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class MinSurfUni : public TutteParameterizer {
public:
    explicit MinSurfUni(std::shared_ptr<PolyMesh> mesh);
    void execute() override;
};

USTC_CG_NAMESPACE_CLOSE_SCOPE