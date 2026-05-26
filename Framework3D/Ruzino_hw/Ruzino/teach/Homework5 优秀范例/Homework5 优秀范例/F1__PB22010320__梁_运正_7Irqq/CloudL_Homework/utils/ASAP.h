#pragma once
#include "ARAP.h"

USTC_CG_NAMESPACE_OPEN_SCOPE

class ASAP : public ARAP {
public:
    explicit ASAP(std::shared_ptr<PolyMesh> mesh, std::shared_ptr<PolyMesh> param_mesh);
    virtual ~ASAP() = default;
    
    // ASAP 特有的执行函数（与 ARAP 的 execute 区分开）
    void execute_asap();
};

USTC_CG_NAMESPACE_CLOSE_SCOPE
