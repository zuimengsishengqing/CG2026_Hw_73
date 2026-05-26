// #define __GNUC__

#include "material.h"

#include <spdlog/spdlog.h>

#include "RHI/internal/map.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hio/image.h"
#include "pxr/usd/sdr/shaderNode.h"
#include "pxr/usd/usd/tokens.h"
#include "pxr/usdImaging/usdImaging/tokens.h"
#include "renderParam.h"
#include "texture.h"
#include "utils/sampling.hpp"

RUZINO_NAMESPACE_OPEN_SCOPE
using namespace pxr;

// Here for the cource purpose, we support a very limited set of forms of the
// material. Specifically, we support only UsdPreviewSurface, and each input can
// be either value, or a texture connected to a primvar reader.

HdMaterialNode2 Hd_RUZINO_Material::get_input_connection(
    HdMaterialNetwork2 surfaceNetwork,
    std::map<TfToken, std::vector<HdMaterialConnection2>>::value_type&
        input_connection)
{
    HdMaterialNode2 upstream;
    assert(input_connection.second.size() == 1);
    upstream = surfaceNetwork.nodes[input_connection.second[0].upstreamNode];
    return upstream;
}

Hd_RUZINO_Material::MaterialRecord Hd_RUZINO_Material::SampleMaterialRecord(
    GfVec2f texcoord)
{
    MaterialRecord ret;
    if (diffuseColor.image) {
        auto val4 = diffuseColor.image->Evaluate(texcoord);
        ret.diffuseColor = { val4[0], val4[1], val4[2] };
    }
    else {
        ret.diffuseColor = diffuseColor.value.Get<GfVec3f>();
    }

    if (roughness.image) {
        auto val4 = roughness.image->Evaluate(texcoord);
        ret.roughness = val4[1];
    }
    else {
        ret.roughness = roughness.value.Get<float>();
    }

    if (ior.image) {
        auto val4 = ior.image->Evaluate(texcoord);
        ret.ior = val4[0];
    }
    else {
        ret.ior = ior.value.Get<float>();
    }

    if (metallic.image) {
        auto val4 = metallic.image->Evaluate(texcoord);
        ret.metallic = val4[2];
    }
    else {
        ret.metallic = metallic.value.Get<float>();
    }

    return ret;
}

void Hd_RUZINO_Material::TryLoadTexture(
    const char* name,
    InputDescriptor& descriptor,
    HdMaterialNode2& usd_preview_surface)
{
    for (auto&& input_connection : usd_preview_surface.inputConnections) {
        if (input_connection.first == TfToken(name)) {
            spdlog::info(
                "Loading texture: " + input_connection.first.GetString());
            auto texture_node =
                get_input_connection(surfaceNetwork, input_connection);
            assert(texture_node.nodeTypeId == UsdImagingTokens->UsdUVTexture);

            auto assetPath =
                texture_node.parameters[TfToken("file")].Get<SdfAssetPath>();

            HioImage::SourceColorSpace colorSpace;

            if (texture_node.parameters[TfToken("sourceColorSpace")] ==
                TfToken("sRGB")) {
                colorSpace = HioImage::SRGB;
            }
            else {
                colorSpace = HioImage::Raw;
            }

            descriptor.image =
                std::make_unique<Texture2D>(assetPath, colorSpace);
            if (!descriptor.image->isValid()) {
                descriptor.image = nullptr;
            }
            descriptor.wrapS =
                texture_node.parameters[TfToken("wrapS")].Get<TfToken>();
            descriptor.wrapT =
                texture_node.parameters[TfToken("wrapT")].Get<TfToken>();

            HdMaterialNode2 st_read_node;
            for (auto&& st_read_connection : texture_node.inputConnections) {
                st_read_node =
                    get_input_connection(surfaceNetwork, st_read_connection);
            }

            assert(
                st_read_node.nodeTypeId ==
                UsdImagingTokens->UsdPrimvarReader_float2);
            descriptor.uv_primvar_name =
                st_read_node.parameters[TfToken("varname")].Get<TfToken>();
            if (descriptor.uv_primvar_name.empty()) {
                descriptor.uv_primvar_name =
                    st_read_node.parameters[TfToken("varname")]
                        .Get<std::string>();
            }
        }
    }
}

void Hd_RUZINO_Material::TryLoadParameter(
    const char* name,
    InputDescriptor& descriptor,
    HdMaterialNode2& usd_preview_surface)
{
    for (auto&& parameter : usd_preview_surface.parameters) {
        if (parameter.first == name) {
            descriptor.value = parameter.second;
            spdlog::info("Loading parameter: " + parameter.first.GetString());
        }
    }
}

#define INPUT_LIST                                                            \
    diffuseColor, specularColor, emissiveColor, displacement, opacity,        \
        opacityThreshold, roughness, metallic, clearcoat, clearcoatRoughness, \
        occlusion, normal, ior

#define TRY_LOAD(INPUT)                                 \
    TryLoadTexture(#INPUT, INPUT, usd_preview_surface); \
    TryLoadParameter(#INPUT, INPUT, usd_preview_surface);

#define NAME_IT(INPUT) INPUT.input_name = TfToken(#INPUT);

Hd_RUZINO_Material::Hd_RUZINO_Material(const SdfPath& id) : HdMaterial(id)
{
    spdlog::info("Creating material " + id.GetString());
    diffuseColor.value = VtValue(GfVec3f(0.8f));
    roughness.value = VtValue(0.8f);

    metallic.value = VtValue(0.0f);
    normal.value = VtValue(GfVec3f(0.5, 0.5, 1.0));
    ior.value = VtValue(1.5f);

    MACRO_MAP(NAME_IT, INPUT_LIST);
}

void Hd_RUZINO_Material::Sync(
    HdSceneDelegate* sceneDelegate,
    HdRenderParam* renderParam,
    HdDirtyBits* dirtyBits)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    VtValue vtMat = sceneDelegate->GetMaterialResource(GetId());
    if (vtMat.IsHolding<HdMaterialNetworkMap>()) {
        const HdMaterialNetworkMap& hdNetworkMap =
            vtMat.UncheckedGet<HdMaterialNetworkMap>();
        if (!hdNetworkMap.terminals.empty() && !hdNetworkMap.map.empty()) {
            spdlog::info("Loaded a material");

            surfaceNetwork = HdConvertToHdMaterialNetwork2(hdNetworkMap);

            // Here we only support single output material.
            assert(surfaceNetwork.terminals.size() == 1);

            auto terminal =
                surfaceNetwork.terminals[HdMaterialTerminalTokens->surface];

            auto usd_preview_surface =
                surfaceNetwork.nodes[terminal.upstreamNode];
            assert(
                usd_preview_surface.nodeTypeId ==
                UsdImagingTokens->UsdPreviewSurface);

            MACRO_MAP(TRY_LOAD, INPUT_LIST)
        }
    }
    else {
        spdlog::info("Not loaded a material");
    }
    *dirtyBits = Clean;
}

HdDirtyBits Hd_RUZINO_Material::GetInitialDirtyBitsMask() const
{
    return AllDirty;
}

#define requireTexCoord(INPUT)            \
    if (!INPUT.uv_primvar_name.empty()) { \
        return INPUT.uv_primvar_name;     \
    }

std::string Hd_RUZINO_Material::requireTexcoordName()
{
    MACRO_MAP(requireTexCoord, INPUT_LIST)
    return {};
}

void Hd_RUZINO_Material::Finalize(HdRenderParam* renderParam)
{
    static_cast<Hd_RUZINO_RenderParam*>(renderParam)->AcquireSceneForEdit();

    HdMaterial::Finalize(renderParam);
}

Color Hd_RUZINO_Material::Sample(
    const GfVec3f& wo,
    GfVec3f& wi,
    float& pdf,
    GfVec2f texcoord,
    const std::function<float()>& uniform_float)
{
    auto record = SampleMaterialRecord(texcoord);
    
    // HW7_TODO: Sample BRDF (optional)
    // 实现Disney BRDF的采样
    // 根据metallic和roughness选择采样策略
    
    float metallic = record.metallic;
    float roughness = record.roughness;
    
    // 使用重要性采样：根据metallic选择漫反射或镜面反射采样
    if (uniform_float() < metallic) {
        // 镜面反射采样（GGX分布）
        auto sample2D = GfVec2f{ uniform_float(), uniform_float() };
        
        // 转换到半程向量空间
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;
        
        // 采样GGX分布
        float phi = 2.0f * M_PI * sample2D[0];
        float cos_theta = std::sqrt((1.0f - sample2D[1]) / (1.0f + (alpha2 - 1.0f) * sample2D[1]));
        float sin_theta = std::sqrt(1.0f - cos_theta * cos_theta);
        
        GfVec3f h(sin_theta * std::cos(phi), sin_theta * std::sin(phi), cos_theta);
        
        // 计算反射方向
        wi = 2.0f * GfDot(wo, h) * h - wo;
        
        // 计算PDF
        float cos_h = std::max(0.0f, h[2]);
        float D = alpha2 / (M_PI * std::pow(cos_h * cos_h * (alpha2 - 1.0f) + 1.0f, 2.0f));
        pdf = D * cos_h / (4.0f * std::max(0.001f, GfDot(wo, h)));
        
    } else {
        // 漫反射采样（余弦加权）
        auto sample2D = GfVec2f{ uniform_float(), uniform_float() };
        wi = CosineWeightedDirection(sample2D, pdf);
    }
    
    // 确保wi在上半球
    if (wi[2] <= 0) {
        pdf = 0;
        return Color{ 0 };
    }
    
    return Eval(wi, wo, texcoord);
}

Color Hd_RUZINO_Material::Eval(GfVec3f wi, GfVec3f wo, GfVec2f texcoord)
{
    auto record = SampleMaterialRecord(texcoord);
    
    // HW7_TODO: Implement Disney BRDF (optional)
    // 实现Disney BRDF的评估
    
    float metallic = record.metallic;
    float roughness = record.roughness;
    GfVec3f diffuseColor = record.diffuseColor;
    
    // 确保方向在上半球
    float cos_wi = std::max(0.0f, wi[2]);
    float cos_wo = std::max(0.0f, wo[2]);
    
    if (cos_wi <= 0 || cos_wo <= 0) {
        return Color{ 0 };
    }
    
    // 计算半程向量
    GfVec3f h = (wi + wo).GetNormalized();
    float cos_h = std::max(0.0f, h[2]);
    
    // Disney BRDF = (1 - metallic) * diffuse + metallic * specular
    
    // 漫反射部分（Lambertian）
    GfVec3f diffuse = diffuseColor / M_PI;
    
    // 镜面反射部分（GGX + Fresnel + Geometry）
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    
    // GGX法线分布函数
    float D = alpha2 / (M_PI * std::pow(cos_h * cos_h * (alpha2 - 1.0f) + 1.0f, 2.0f));
    
    // Fresnel项（Schlick近似）
    float F0 = 0.04f * (1.0f - metallic) + metallic;
    float F = F0 + (1.0f - F0) * std::pow(1.0f - std::max(0.0f, GfDot(wi, h)), 5.0f);
    
    // 几何遮蔽函数（Smith GGX）
    float lambda_wi = (-1.0f + std::sqrt(1.0f + alpha2 * (1.0f - cos_wi * cos_wi) / (cos_wi * cos_wi))) / 2.0f;
    float lambda_wo = (-1.0f + std::sqrt(1.0f + alpha2 * (1.0f - cos_wo * cos_wo) / (cos_wo * cos_wo))) / 2.0f;
    float G = 1.0f / (1.0f + lambda_wi + lambda_wo);
    
    // 镜面反射BRDF
    GfVec3f specular = GfVec3f(D * F * G / (4.0f * cos_wi * cos_wo));
    
    // 组合漫反射和镜面反射
    GfVec3f result = (1.0f - metallic) * diffuse + specular;
    
    return result;
}

float Hd_RUZINO_Material::Pdf(GfVec3f wi, GfVec3f wo, GfVec2f texcoord)
{
    auto record = SampleMaterialRecord(texcoord);
    
    // HW7_TODO: Calculate PDF for Disney BRDF (optional)
    // 计算Disney BRDF的PDF
    
    float metallic = record.metallic;
    float roughness = record.roughness;
    
    // 确保方向在上半球
    float cos_wi = std::max(0.0f, wi[2]);
    float cos_wo = std::max(0.0f, wo[2]);
    
    if (cos_wi <= 0 || cos_wo <= 0) {
        return 0;
    }
    
    // 计算半程向量
    GfVec3f h = (wi + wo).GetNormalized();
    float cos_h = std::max(0.0f, h[2]);
    
    // 漫反射PDF（余弦加权）
    float pdf_diffuse = cos_wi / M_PI;
    
    // 镜面反射PDF（GGX分布）
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float D = alpha2 / (M_PI * std::pow(cos_h * cos_h * (alpha2 - 1.0f) + 1.0f, 2.0f));
    float pdf_specular = D * cos_h / (4.0f * std::max(0.001f, GfDot(wo, h)));
    
    // 组合PDF（根据metallic权重）
    float pdf = (1.0f - metallic) * pdf_diffuse + metallic * pdf_specular;
    
    return pdf;
}

RUZINO_NAMESPACE_CLOSE_SCOPE