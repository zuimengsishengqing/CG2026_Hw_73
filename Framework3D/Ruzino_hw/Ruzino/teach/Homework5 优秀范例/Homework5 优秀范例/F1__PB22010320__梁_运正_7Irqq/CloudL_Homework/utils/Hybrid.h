#pragma
#include "ARAP.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class Hybrid : public ARAP{
public:
    explicit Hybrid(
        std::shared_ptr<PolyMesh> mesh,
        std::shared_ptr<PolyMesh> param_mesh,
        float lambda = 1.0f  // Hybrid参数λ
    );

    void set_lambda(float lambda) { lambda_ = lambda; }
    float get_lambda() const { return lambda_; }

    protected:
    void local_phase() override;  // 覆盖ARAP的局部阶段
    float newton_method(float alpha, float beta, float gamma, float a_init);
    float lambda_;
};

USTC_CG_NAMESPACE_CLOSE_SCOPE