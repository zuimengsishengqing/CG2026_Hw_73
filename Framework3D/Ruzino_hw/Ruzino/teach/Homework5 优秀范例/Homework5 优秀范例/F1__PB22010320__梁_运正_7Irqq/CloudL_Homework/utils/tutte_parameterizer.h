// utils/tutte_parameterizer.h
#pragma once
#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include <OpenMesh/Core/Mesh/PolyMesh_ArrayKernelT.hh>
#include <OpenMesh/Core/Geometry/VectorT.hh>
#include <Eigen/Sparse>
#include <memory>
#include <vector>
#include <cmath>


USTC_CG_NAMESPACE_OPEN_SCOPE

class TutteParameterizer {
public:
    explicit TutteParameterizer(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> original = nullptr);
    virtual ~TutteParameterizer() = default;

    virtual void execute() = 0;
    std::shared_ptr<Geometry> get_result() const;  // 修改为 shared_ptr

protected:
    std::shared_ptr<PolyMesh> mesh_;
    std::shared_ptr<PolyMesh> original_mesh_;
    std::vector<int> boundary_indices_;
    std::vector<bool> is_boundary_;

    void detect_boundaries();
    void update_mesh(const Eigen::VectorXd& x, const Eigen::VectorXd& y, const Eigen::VectorXd& z);
};

USTC_CG_NAMESPACE_CLOSE_SCOPE