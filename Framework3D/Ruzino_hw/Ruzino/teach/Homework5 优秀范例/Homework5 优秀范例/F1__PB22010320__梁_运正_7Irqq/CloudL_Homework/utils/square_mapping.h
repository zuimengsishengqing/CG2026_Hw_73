// utils/square_mapping.h
#pragma once
#include "tutte_parameterizer.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class SquareMapping : public TutteParameterizer {
public:
    explicit SquareMapping(std::shared_ptr<PolyMesh> mesh);
    void execute() override;
};

USTC_CG_NAMESPACE_CLOSE_SCOPE