#include "light.h"

#include <spdlog/spdlog.h>

#include "pxr/base/gf/plane.h"
#include "pxr/base/gf/ray.h"
#include "pxr/base/gf/rotation.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/imaging/glf/simpleLight.h"
#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/rprimCollection.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hio/image.h"
#include "renderParam.h"
#include "texture.h"
#include "utils/math.hpp"
#include "utils/sampling.hpp"

RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;
void Hd_RUZINO_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    TF_UNUSED(renderParam);

    if (!TF_VERIFY(sceneDelegate != nullptr)) {
        return;
    }

    const SdfPath& id = GetId();

    // Change tracking
    HdDirtyBits bits = *dirtyBits;

    // Transform
    if (bits & DirtyTransform) {
        _params[HdTokens->transform] = VtValue(sceneDelegate->GetTransform(id));
    }

    // Lighting Params
    if (bits & DirtyParams) {
        HdChangeTracker& changeTracker =
            sceneDelegate->GetRenderIndex().GetChangeTracker();

        // Remove old dependencies
        VtValue val = Get(HdTokens->filters);
        if (val.IsHolding<SdfPathVector>()) {
            auto lightFilterPaths = val.UncheckedGet<SdfPathVector>();
            for (const SdfPath& filterPath : lightFilterPaths) {
                changeTracker.RemoveSprimSprimDependency(filterPath, id);
            }
        }

        if (_lightType == HdPrimTypeTokens->simpleLight) {
            _params[HdLightTokens->params] =
                sceneDelegate->Get(id, HdLightTokens->params);
        }

        // Add new dependencies
        val = Get(HdTokens->filters);
        if (val.IsHolding<SdfPathVector>()) {
            auto lightFilterPaths = val.UncheckedGet<SdfPathVector>();
            for (const SdfPath& filterPath : lightFilterPaths) {
                changeTracker.AddSprimSprimDependency(filterPath, id);
            }
        }
    }

    *dirtyBits = Clean;
}

HdDirtyBits Hd_RUZINO_Light::GetInitialDirtyBitsMask() const
{
    if (_lightType == HdPrimTypeTokens->simpleLight ||
        _lightType == HdPrimTypeTokens->distantLight) {
        return AllDirty;
    }
    else {
        return (DirtyParams | DirtyTransform);
    }
}

bool Hd_RUZINO_Light::IsDomeLight()
{
    return _lightType == HdPrimTypeTokens->domeLight;
}

void Hd_RUZINO_Light::Finalize(HdRenderParam* renderParam)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    HdLight::Finalize(renderParam);
}

VtValue Hd_RUZINO_Light::Get(const TfToken& token) const
{
    VtValue val;
    TfMapLookup(_params, token, &val);
    return val;
}

Color Hd_RUZINO_Sphere_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,

    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    auto distanceVec = position - pos;

    auto basis = constructONB(-distanceVec.GetNormalized());

    auto distance = distanceVec.GetLength();

    // A sphere light is treated as all points on the surface spreads energy
    // uniformly:
    float sample_pos_pdf;
    // First we sample a point on the hemi sphere:
    auto sampledDir = CosineWeightedDirection(
        GfVec2f(uniform_float(), uniform_float()), sample_pos_pdf);
    auto worldSampledDir = basis * sampledDir;

    auto sampledPosOnSurface = worldSampledDir * radius + position;
    sampled_light_pos = sampledPosOnSurface;

    // Then we can decide the direction.
    dir = (sampledPosOnSurface - pos).GetNormalized();

    // and the pdf (with the measure of solid angle):
    float cosVal = GfDot(-dir, worldSampledDir.GetNormalized());

    sample_light_pdf =
        sample_pos_pdf / radius / radius / cosVal * distance * distance;

    // Finally we calculate the radiance
    if (cosVal < 0) {
        return Color{ 0 };
    }
    return irradiance / M_PI;
}

Color Hd_RUZINO_Sphere_Light::Intersect(const GfRay& ray, float& depth)
{
    double distance;
    if (ray.Intersect(
            GfRange3d{ position - GfVec3d{ radius },
                       position + GfVec3d{ radius } })) {
        if (ray.Intersect(position, radius, &distance)) {
            depth = distance;

            return irradiance / M_PI;
        }
    }
    depth = std::numeric_limits<float>::infinity();
    return { 0, 0, 0 };
}

void Hd_RUZINO_Sphere_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    auto id = GetId();

    radius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius)
                 .Get<float>();

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();

    auto intensity =
        sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity)
            .GetWithDefault<float>();
    power = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                .Get<GfVec3f>() *
            diffuse * intensity;

    auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();

    GfVec3d p = transform.ExtractTranslation();
    position = GfVec3f(p[0], p[1], p[2]);

    area = 4 * M_PI * radius * radius;

    irradiance = power / area;
}

Color Hd_RUZINO_Dome_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,
    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    dir = UniformSampleSphere(
        GfVec2f{ uniform_float(), uniform_float() }, sample_light_pdf);
    sampled_light_pos = dir * std::numeric_limits<float>::max() / 100.f;

    return Le(dir);
}

Color Hd_RUZINO_Dome_Light::Intersect(const GfRay& ray, float& depth)
{
    depth = 10000000.f;
    return Le(GfVec3f(ray.GetDirection()).GetNormalized());
}

void Hd_RUZINO_Dome_Light::_PrepareDomeLight(
    SdfPath const& id,
    HdSceneDelegate* sceneDelegate)
{
    const VtValue v =
        sceneDelegate->GetLightParamValue(id, HdLightTokens->textureFile);
    if (!v.IsEmpty()) {
        if (v.IsHolding<SdfAssetPath>()) {
            textureFileName = v.UncheckedGet<SdfAssetPath>();
            texture = std::make_unique<Texture2D>(textureFileName);
            if (!texture->isValid()) {
                texture = nullptr;
            }

            spdlog::info(
                ("Attempting to load file " + textureFileName.GetAssetPath())
                    .c_str());
        }
        else {
            texture = nullptr;
        }
    }
    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    radiance = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                   .Get<GfVec3f>() *
               diffuse;
}

void Hd_RUZINO_Dome_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);

    auto id = GetId();
    _PrepareDomeLight(id, sceneDelegate);
}

Color Hd_RUZINO_Dome_Light::Le(const GfVec3f& dir)
{
    if (texture != nullptr) {
        auto uv = GfVec2f(
            (M_PI + std::atan2(dir[1], dir[0])) / 2.0 / M_PI,
            0.5 - dir[2] * 0.5);

        auto value = texture->Evaluate(uv);

        if (texture->component_conut() >= 3) {
            return GfCompMult(Color{ value[0], value[1], value[2] }, radiance);
        }
    }
    return radiance;
}

void Hd_RUZINO_Dome_Light::Finalize(HdRenderParam* renderParam)
{
    texture = nullptr;
    Hd_RUZINO_Light::Finalize(renderParam);
}

// HW7_TODO: write the following, you should refer to the sphere light.

void Hd_RUZINO_Distant_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);
    auto id = GetId();
    angle = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle)
                .Get<float>();
    angle = std::clamp(angle, 0.03f, 89.9f) * M_PI / 180.0f;

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    radiance = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                   .Get<GfVec3f>() *
               diffuse / (1 - cos(angle)) / 2.0 / M_PI;

    auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();

    direction =
        GfVec3f(transform.TransformDir(GfVec3f(0, 0, -1)).GetNormalized());
}

Color Hd_RUZINO_Distant_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,
    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    float theta = uniform_float() * angle;
    float phi = uniform_float() * 2 * M_PI;

    auto sampled_dir =
        GfVec3f(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));

    auto basis = constructONB(-direction);

    dir = basis * sampled_dir;
    sampled_light_pos = pos + dir * 10000000.f;

    sample_light_pdf = 1.0f / sin(theta) / (2.0f * M_PI * angle);

    return radiance;
}

Color Hd_RUZINO_Distant_Light::Intersect(const GfRay& ray, float& depth)
{
    depth = 10000000.f;

    if (GfDot(ray.GetDirection().GetNormalized(), -direction) > cos(angle)) {
        return radiance;
    }
    return Color(0);
}

Color Hd_RUZINO_Rect_Light::Sample(
    const GfVec3f& pos,
    GfVec3f& dir,
    GfVec3f& sampled_light_pos,
    float& sample_light_pdf,
    const std::function<float()>& uniform_float)
{
    // HW7_TODO: implement the sample function for rectangle light, you should refer to the sphere light.
    // 矩形光源采样：在矩形面上均匀采样一个点
    float u = uniform_float();
    float v = uniform_float();
    
    // 使用双线性插值计算采样点位置
    GfVec3f sampledPos = (1 - u) * (1 - v) * corner0 + 
                         (1 - u) * v * corner1 + 
                         u * (1 - v) * corner2 + 
                         u * v * corner3;
    
    sampled_light_pos = sampledPos;
    
    // 计算从采样点到光源的方向
    dir = (sampledPos - pos).GetNormalized();
    
    // 计算矩形光源的法线（通过叉积）
    GfVec3f edge1 = corner1 - corner0;
    GfVec3f edge2 = corner2 - corner0;
    GfVec3f normal = GfCross(edge1, edge2).GetNormalized();
    
    // 计算距离和面积
    float distance = (sampledPos - pos).GetLength();
    float area = GfCross(edge1, edge2).GetLength();
    
    // 计算cos值（光源法线与采样方向的夹角）
    float cosVal = GfDot(-dir, normal);
    
    // 如果cosVal < 0，说明采样点在光源背面
    if (cosVal <= 0) {
        sample_light_pdf = 0;
        return Color{ 0 };
    }
    
    // 计算PDF（立体角测度）：pdf_area * distance^2 / cosVal
    // 其中pdf_area = 1/area（均匀采样）
    sample_light_pdf = distance * distance / (area * cosVal);
    
    // 返回辐照度/π（假设是Lambertian光源）
    float inv_area_pi = 1.0f / (area * static_cast<float>(M_PI));
    return GfVec3f(power[0] * inv_area_pi, power[1] * inv_area_pi, power[2] * inv_area_pi);
}

// HW7_TODO: implement the intersect function for rectangle light, you can refer to the sphere light, but you need to consider the fact that rectangle light is not a point light source.
Color Hd_RUZINO_Rect_Light::Intersect(const GfRay& ray, float& depth)
{
    // 计算矩形光源的法线
    GfVec3f edge1 = corner1 - corner0;
    GfVec3f edge2 = corner2 - corner0;
    GfVec3f normal = GfCross(edge1, edge2).GetNormalized();
    
    // 计算射线与平面的交点
    float denom = GfDot(ray.GetDirection(), normal);
    
    // 如果射线与平面平行，不相交
    if (std::abs(denom) < 1e-6f) {
        depth = std::numeric_limits<float>::infinity();
        return Color{ 0 };
    }
    
    // 计算交点距离
    float t = GfDot(corner0 - ray.GetStartPoint(), normal) / denom;
    
    // 如果交点在射线后方，不相交
    if (t <= 0) {
        depth = std::numeric_limits<float>::infinity();
        return Color{ 0 };
    }
    
    // 计算交点位置
    GfVec3f hitPoint = GfVec3f(ray.GetPoint(t));
    
    // 检查交点是否在矩形内
    GfVec3f v0 = hitPoint - corner0;
    GfVec3f v1 = corner1 - corner0;
    GfVec3f v2 = corner2 - corner0;
    
    float dot00 = GfDot(v0, v0);
    float dot01 = GfDot(v0, v1);
    float dot02 = GfDot(v0, v2);
    float dot11 = GfDot(v1, v1);
    float dot12 = GfDot(v1, v2);
    
    // 计算重心坐标
    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;
    
    // 检查重心坐标是否在[0,1]范围内
    if (u >= 0 && u <= 1 && v >= 0 && v <= 1 && u + v <= 1) {
        depth = t;
        float area = GfCross(edge1, edge2).GetLength();
        float inv_area_pi = 1.0f / (area * static_cast<float>(M_PI));
        return GfVec3f(power[0] * inv_area_pi, power[1] * inv_area_pi, power[2] * inv_area_pi);
       }
    
    depth = std::numeric_limits<float>::infinity();
    return Color{ 0 };
}

void Hd_RUZINO_Rect_Light::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    Hd_RUZINO_Light::Sync(sceneDelegate, renderParam, dirtyBits);

    auto transform = Get(HdTokens->transform).GetWithDefault<GfMatrix4d>();

    auto id = GetId();
    width = sceneDelegate->GetLightParamValue(id, HdLightTokens->width)
                .Get<float>();
    height = sceneDelegate->GetLightParamValue(id, HdLightTokens->height)
                 .Get<float>();

    corner0 = GfVec3f(
        transform.TransformAffine(GfVec3f(-0.5 * width, -0.5 * height, 0)));
    corner1 = GfVec3f(
        transform.TransformAffine(GfVec3f(-0.5 * width, 0.5 * height, 0)));
    corner2 = GfVec3f(
        transform.TransformAffine(GfVec3f(0.5 * width, -0.5 * height, 0)));
    corner3 = GfVec3f(
        transform.TransformAffine(GfVec3f(0.5 * width, 0.5 * height, 0)));

    auto diffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse)
                       .Get<float>();
    power = sceneDelegate->GetLightParamValue(id, HdLightTokens->color)
                .Get<GfVec3f>() *
            diffuse;

    // HW7_TODO: calculate irradiance
    // 计算矩形光源的面积
    GfVec3f edge1 = corner1 - corner0;
    GfVec3f edge2 = corner2 - corner0;
    float area = GfCross(edge1, edge2).GetLength();
    
    // Irradiance = Power / Area
    // 注意：这里irradiance变量没有在类中定义，如果需要使用，需要在类中添加成员变量
    irradiance = GfVec3f(power[0] / area, power[1] / area, power[2] / area);
}

RUZINO_NAMESPACE_CLOSE_SCOPE