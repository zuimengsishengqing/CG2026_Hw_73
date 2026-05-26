#pragma once
#include "GCore/Components/MeshOperand.h"
#include "GCore/util_openmesh_bind.h"
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <memory>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

USTC_CG_NAMESPACE_OPEN_SCOPE

class ARAP {
public:
    explicit ARAP(std::shared_ptr<PolyMesh> mesh, 
                 std::shared_ptr<PolyMesh> param_mesh);
    virtual ~ARAP() = default;

    virtual void execute(int iterations);
    void precompute();
    pxr::VtArray<pxr::GfVec2f> get_result() const;
    void reset();  
    bool needs_precompute(const std::shared_ptr<PolyMesh>& new_mesh, 
        const std::shared_ptr<PolyMesh>& new_param_mesh) const;

protected:
    std::shared_ptr<PolyMesh> mesh_;
    std::shared_ptr<PolyMesh> param_mesh_;
    int iterations_;
    Eigen::SparseMatrix<float> A_;              // 系数矩阵
    std::vector<float> ctg_;              // 余切权重
    bool precomputed_ = false;
    Eigen::MatrixXf u_;                     // 计算结果
    std::vector<Eigen::Matrix2f> transforms;           // 每面片的旋转矩阵
    std::vector<Eigen::Matrix2f> x_coords_;           // 初始三角形坐标
    std::vector<Eigen::Matrix2f> u_coords_;           // 当前参数化坐标
    std::vector<Eigen::Vector2f> x_;       // 半边向量
    std::vector<Eigen::Triplet<float>> triplets_;
    Eigen::SimplicialLDLT<Eigen::SparseMatrix<float>> solver_;
    void initialize_solver();
    void initialize_param();
    void ARAP_iteration();
    virtual void local_phase();
    void global_phase();

};

USTC_CG_NAMESPACE_CLOSE_SCOPE