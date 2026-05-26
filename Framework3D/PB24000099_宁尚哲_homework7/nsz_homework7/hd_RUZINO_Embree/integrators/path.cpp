#include "path.h"

#include <random>

#include "../surfaceInteraction.h"
RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;

VtValue PathIntegrator::Li(const GfRay& ray, std::default_random_engine& random)
{
    std::uniform_real_distribution<float> uniform_dist(
        0.0f, 1.0f - std::numeric_limits<float>::epsilon());
    std::function<float()> uniform_float = std::bind(uniform_dist, random);

    auto color = EstimateOutGoingRadiance(ray, uniform_float, 0);

    return VtValue(GfVec3f(color[0], color[1], color[2]));
}

GfVec3f PathIntegrator::EstimateOutGoingRadiance(
    const GfRay& ray,
    const std::function<float()>& uniform_float,
    int recursion_depth)
{
    if (recursion_depth >= 50) {
        return {};
    }

    SurfaceInteraction si;
    if (!Intersect(ray, si)) {
        if (recursion_depth == 0) {
            return IntersectDomeLight(ray);
        }

        return GfVec3f{ 0, 0, 0 };
    }

    // This can be customized : Do we want to see the lights? (Other than dome
    // lights?)
    if (recursion_depth == 0) {
    }

    // Flip the normal if opposite
    if (GfDot(si.shadingNormal, ray.GetDirection()) > 0) {
        si.flipNormal();
        si.PrepareTransforms();
    }

    GfVec3f color{ 0 };
    GfVec3f directLight = EstimateDirectLight(si, uniform_float);

    // HW7_TODO: Estimate global lighting here.
    GfVec3f globalLight = GfVec3f{ 0.f };
    
    // 实现路径追踪的间接光照计算
    // 1. 从BRDF采样一个新的方向
    GfVec3f wi;
    float brdf_pdf;
    GfVec3f sampled_light_pos;
    
    // 使用SurfaceInteraction的Sample方法进行BRDF采样
    auto brdf_val = si.Sample(wi, brdf_pdf, uniform_float);
    
    // 2. 检查采样是否有效
    if (brdf_pdf > 0 && GfDot(wi, si.shadingNormal) > 0) {
        // 3. 构造新的光线进行递归
        GfRay indirect_ray(si.position + 0.0001f * si.geometricNormal, wi);
        
        // 4. 递归计算间接光照
        GfVec3f indirect_radiance = EstimateOutGoingRadiance(indirect_ray, uniform_float, recursion_depth + 1);
        
        // 5. 计算间接光照的贡献
        // 使用渲染方程：L = f_r * L_i * cos(theta) / pdf
        float cos_theta = GfDot(wi, si.shadingNormal);
        globalLight = GfCompMult(brdf_val, indirect_radiance) * cos_theta / brdf_pdf;
    }
    
    // 6. Russian Roulette：根据递归深度决定是否继续追踪
    if (recursion_depth >= 3) {
        // 计算继续概率（基于颜色的最大分量）
        float max_component = std::max({ color[0], color[1], color[2] });
        float continue_prob = std::min(0.95f, max_component);
        
        if (uniform_float() > continue_prob) {
            // 终止追踪，返回黑色
            return directLight;
        }
        
        // 如果继续追踪，需要除以继续概率以保持无偏估计
        color = (directLight + globalLight) / continue_prob;
    } else {
        color = directLight + globalLight;
    }

    return color;
}

RUZINO_NAMESPACE_CLOSE_SCOPE