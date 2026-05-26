###### 导入

![image-20260422164741203](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260422164741203.png)

###### 节点连接图

![image-20260422165559579](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260422165559579.png)

光源一开始看不到效果，因为node_render_deferred_lighting_gl.cpp没有改

##### ❌ **核心问题：光源矩阵未填充**

查看 `node_render_deferred_lighting_gl.cpp` 代码，发现：



```
light_vector.emplace_back(
    GfMatrix4f(), GfMatrix4f(), position3, 0.f, diffuse3, i);
```

**问题**：`light_projection` 和 `light_view` 都是**单位矩阵**（identity matrix）！

#### 修复后，添加了V矩阵效果图

位置图

![image-20260422225642258](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260422225642258.png)

添加部分代码：

`node_render_deferred_lighting_gl.cpp`

```
                // 计算光源的 view 和 projection 矩阵
                GfMatrix4f light_view_mat;
                GfMatrix4f light_projection_mat;
                bool has_light = false;

                if (lights[i]->GetLightType() == HdPrimTypeTokens->sphereLight) {
                    GfFrustum frustum;
                    GfVec3f light_position = position3;

                    // 计算光源视图矩阵（从光源看向原点）
                    light_view_mat = GfMatrix4f().SetLookAt(
                        light_position, GfVec3f(0, 0, 0), GfVec3f(0, 0, 1));

                    // 计算光源投影矩阵（透视投影）
                    frustum.SetPerspective(120.f, 1.0, 1, 25.f);
                    light_projection_mat = GfMatrix4f(frustum.ComputeProjectionMatrix());

                    has_light = true;
                }

                if (!has_light) {
                    continue;
                }
```

##### 1. **添加光源矩阵计算**

```
// 计算光源的 view 和 projection 矩阵
GfMatrix4f light_view_mat;
GfMatrix4f light_projection_mat;
bool has_light = false;
```

##### 2. **计算 Sphere Light 的矩阵**

```
if (lights[i]->GetLightType() == HdPrimTypeTokens->sphereLight) {
    GfFrustum frustum;
    GfVec3f light_position = position3;
    
    // 计算光源视图矩阵（从光源看向原点）
    light_view_mat = GfMatrix4f().SetLookAt(
        light_position, GfVec3f(0, 0, 0), GfVec3f(0, 0, 1));
    
    // 计算光源投影矩阵（透视投影）
    frustum.SetPerspective(120.f, 1.0, 1, 25.f);
    light_projection_mat = GfMatrix4f(frustum.ComputeProjectionMatrix());
    
    has_light = true;
}
```

##### 3. **传递正确的矩阵**

```
if (!has_light) {
    continue;  // 跳过不支持的光源类型
}

light_vector.emplace_back(
    light_projection_mat,  // ✅ 正确的投影矩阵
    light_view_mat,      // ✅ 正确的视图矩阵
    position3, radius, diffuse3, i);
```

### Shadow Map

#### 硬阴影

### 核心步骤：

1. 将当前像素的世界位置转换到光源空间
2. 采样阴影深度图
3. 比较当前深度与深度图值
4. 计算阴影因子

| ``                                                           |
| ------------------------------------------------------------ |
| `// 4. 组合光照结果（环境光 + 漫反射 + 高光） `              |
| `vec3 result = ambient + diffuse + specular; `               |
| ``                                                           |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
|                                                              |
| `// 5. 应用阴影（硬阴影实现） `                              |
| `// 将当前像素的世界位置转换到光源空间 `                     |
| `vec4 lightSpacePos = lights[i].light_projection * lights[i].light_view * vec4(pos, 1.0); ` |
| ``                                                           |
| `// 执行透视除法，得到 NDC 坐标 `                            |
| `vec3 lightSpaceNDC = lightSpacePos.xyz / lightSpacePos.w; ` |
| ``                                                           |
| `// 将 NDC 坐标从 [-1, 1] 映射到 [0, 1]（纹理坐标范围） `    |
| `vec2 shadowUV = lightSpaceNDC.xy * 0.5 + 0.5; `             |
| ``                                                           |
| `// 当前像素在光源空间的深度值 `                             |
| `float currentDepth = lightSpaceNDC.z; `                     |
| ``                                                           |
| `// 采样阴影深度图 `                                         |
| `float shadowDepth = texture(shadow_maps, vec3(shadowUV, lights[i].shadow_map_id)).x; ` |
| ``                                                           |
| `// 计算阴影因子：如果当前深度大于阴影深度，则在阴影中 `     |
| `// 添加小的偏移量（0.005）避免阴影痤疮（shadow acne） `     |
| `float shadowFactor = (currentDepth > shadowDepth + 0.005) ? 0.0 : 1.0; ` |
| ``                                                           |
| `// 6. 应用阴影因子 `                                        |
| `result *= shadowFactor; `                                   |
| ``                                                           |
| `// 7. 累加到最终颜色（多个光源） `                          |



##### 📝 实现原理

#### **Shadow Mapping 核心思想**：

1. **深度图生成**（shadow_mapping 节点）：
   - 从光源视角渲染场景
   - 记录每个像素的深度值（距离光源的距离）
   - 存储在阴影深度图中
2. **阴影测试**（blinn_phong 节点）：
   - 将当前像素的世界坐标转换到光源空间
   - 采样阴影深度图，获取该位置的最小深度
   - 比较当前像素深度与阴影深度
   - 如果当前深度更大，说明被遮挡，处于阴影中

##### 硬阴影效果图

![image-20260422234219210](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260422234219210.png)

###### 优化实现代码：

```
修复方案

blinn_phong.fs
Apply
vec4 metalnessRoughness = texture2D(metallicRoughnessSampler,uv);
float metal = metalnessRoughness.x;
float roughness = metalnessRoughness.y;
Color = vec4(0, 0, 0, 1);
for(int i = 0; i < light_count; i ++) {
// Blinn-Phong 着色模型实现
// 从 G-Buffer 读取反照率（漫反射颜色）
vec3 albedo = texture2D(diffuseColorSampler, uv).xyz;
// 计算光照向量
vec3 lightDir = normalize(lights[i].position - pos);
vec3 viewDir = normalize(camPos - pos);
vec3 halfDir = normalize(lightDir + viewDir);
// Blinn-Phong 光照计算
// 1. 环境光分量（简化为固定比例的反照率）
vec3 ambient = 0.1 * albedo;
// 将 NDC 坐标从 [-1, 1] 映射到 [0, 1]（纹理坐标范围）
vec2 shadowUV = lightSpaceNDC.xy * 0.5 + 0.5;
// 将当前像素的 NDC 深度也映射到 [0, 1] 范围，与阴影深度图保持一致
float currentDepth = lightSpaceNDC.z * 0.5 + 0.5;
// 初始化阴影因子为 1.0（不在阴影中）
float shadowFactor = 1.0;
// 检查是否在阴影贴图的有效范围内
if (shadowUV.x >= 0.0 && shadowUV.x <= 1.0 &&
    shadowUV.y >= 0.0 && shadowUV.y <= 1.0 &&
    lightSpaceNDC.z >= -1.0 && lightSpaceNDC.z <= 1.0) {
    
    // 采样阴影深度图
    float shadowDepth = texture(shadow_maps, vec3(shadowUV, lights[i].shadow_map_id)).x;
    
    // 计算阴影因子：如果当前深度大于阴影深度，则在阴影中
    // 添加小的偏移量（0.005）避免阴影痤疮（shadow acne）
    shadowFactor = (currentDepth > shadowDepth + 0.005) ? 0.0 : 1.0;
}
// 6. 应用阴影因子
result *= shadowFactor;
现在修复 shadow_mapping.fs 文件，确保深度值正确映射：


shadow_mapping.fs
Apply
layout(location = 0) out float shadow_map0;
void main() {
    vec4 clipPos = light_projection * light_view * (vec4(vertexPosition, 1.0));
    
    // 执行透视除法，得到 NDC 深度值 [-1, 1]
    float ndcDepth = clipPos.z / clipPos.w;
    
    // 将 NDC 深度映射到 [0, 1] 范围，以匹配纹理存储格式
    shadow_map0 = ndcDepth * 0.5 + 0.5;
}
```

###### 窗帘阴影

![image-20260423161125771](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423161125771.png)

###### 狭长阴影：

![image-20260423162313598](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423162313598.png)

###### 环境map

![image-20260424225243844](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424225243844.png)





## 选做部分

软阴影：

Percentage Close Soft Shadow 实现



![image-20260423224933234](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423224933234.png)





##### SSAO

- **新增着色器**: [ssao.fs](vscode-file://vscode-app/e:/java/Microsoft VS Code/10c8e557c8/resources/app/out/vs/code/electron-browser/workbench/workbench.html) — 按 LearnOpenGL 教程实现的 SSAO（kernel、noise、TBN、投影、遮蔽累积）。
- **新增着色器**: [ssao_blur.fs](vscode-file://vscode-app/e:/java/Microsoft VS Code/10c8e557c8/resources/app/out/vs/code/electron-browser/workbench/workbench.html) — 简单 3x3 模糊。
- **修改节点**: [node_render_ssao_gl.cpp](vscode-file://vscode-app/e:/java/Microsoft VS Code/10c8e557c8/resources/app/out/vs/code/electron-browser/workbench/workbench.html) — 在节点端生成 64 个 kernel、4x4 noise 纹理并上传，绑定 G-buffer（Position/Depth），执行 SSAO pass 与 blur pass，输出模糊后的 AO 纹理。

新的节点连线图：

![image-20260424230352505](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424230352505.png)

效果图：

纯白色风格

![image-20260424230114565](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424230114565.png)



效果一览：存在噪点，总体明亮

##### Transfer效果图

![image-20260424230806932](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424230806932.png)

###### 连线：

![image-20260424230822602](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424230822602.png)

##### Displace 部分

更丰富的凹凸不平纹理质感：

![image-20260424232331942](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424232331942.png)

###### 验证纹理凹陷情况图

![image-20260424233416738](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424233416738.png)

###### 节点调参暴露

![image-20260424233438016](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424233438016.png)

暴露了Displacement Scale和Bias

# 光追部分

基础光照配置

![image-20260423180035981](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423180035981.png)

实现基础光追后：

![image-20260423200533181](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423200533181.png)

###### 测上方视图

![image-20260423200933092](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423200933092.png)

###### 调整光强与半径效果图：

![image-20260423202730458](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423202730458.png)





##### 矩形光源效果图

![image-20260424235530680](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424235530680.png)

### 1. [material.cpp](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/宁尚哲/.vscode/extensions/marscode.marscode-extension-1.6.22/) - Disney BRDF模型

#### **Sample函数** - BRDF采样

```
// 根据metallic和roughness选择采样策略
if (uniform_float() < metallic) {
    // 镜面反射采样（GGX分布）
    // 采样半程向量h，计算反射方向wi
    // PDF = D * cos_h / (4 * (wo·h))
} else {
    // 漫反射采样（余弦加权）
    wi = CosineWeightedDirection(sample2D, pdf);
}
```

#### **Eval函数** - Disney BRDF评估

```
// Disney BRDF = (1 - metallic) * diffuse + metallic * specular

// 漫反射部分（Lambertian）
diffuse = diffuseColor / π

// 镜面反射部分（GGX + Fresnel + Geometry）
D = GGX法线分布函数
F = Schlick Fresnel近似
G = Smith GGX几何遮蔽函数
specular = D * F * G / (4 * cos_wi * cos_wo)
```

#### **Pdf函数** - PDF计算

```
// 组合PDF（根据metallic权重）
pdf = (1 - metallic) * pdf_diffuse + metallic * pdf_specular
```

### 2. [integrator.cpp](https://file+.vscode-resource.vscode-cdn.net/c%3A/Users/宁尚哲/.vscode/extensions/marscode.marscode-extension-1.6.22/) - MIS实现

#### **BRDF采样 + MIS**

```
// 1. 从BRDF采样方向
si.Sample(wi_brdf, brdf_pdf, uniform_float)

// 2. 测试可见性并检查是否击中光源
light_radiance = IntersectLights(brdf_ray, intersectPos)

// 3. 计算光源采样的PDF
light_pdf = SampleLights的PDF

// 4. 使用Power Heuristic计算MIS权重
weight = PowerHeuristic(brdf_pdf, light_pdf)

// 5. 组合两种采样的贡献
result = contribution_by_sample_lights * light_weight + contribution_by_sample_brdf
```

### 技术要点

### **Disney BRDF特点**

1. **基于物理的材质模型**：支持metallic和roughness参数
2. **能量守恒**：漫反射和镜面反射通过metallic权重混合
3. **重要性采样**：根据材质特性选择最优采样策略

### **MIS（Multiple Importance Sampling）优势**

1. **减少方差**：结合光源采样和BRDF采样的优点
2. **自适应权重**：使用Power Heuristic根据PDF动态调整权重
3. **无偏估计**：保持渲染结果的正确性

### **Power Heuristic公式**

```
PowerHeuristic(f, g) = f² / (f² + g²)
```

- f: 当前采样方法的PDF
- g: 另一种采样方法的PDF

### 效果预期

实现后你应该能看到：

1. **更真实的金属材质**：高光更锐利，反射更准确
2. **更柔和的漫反射**：粗糙度影响光照分布
3. **更低的噪声**：MIS减少渲染噪声
4. **更好的收敛性**：相同SPP下获得更高质量的结果

##### 基于上面BRDF与 MIS效果图

![image-20260423205356335](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423205356335.png)

可以观察到球材质表面明显接收到了四面八方场景并且做出了反射，我们从材质球上面可以看出边缘的立方体、红色的强以及绿色的墙，甚至是我们摄像机这边的”黑色“（没有墙）

###### 其他角度

![image-20260423213435685](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423213435685.png)

###### MIS 效果图

![image-20260423215441926](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423215441926.png)

还是存在噪声点，这个就是概率模型光线没有计算到采样值

###### Sphere diffuse 0.8

![image-20260423215759151](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423215759151.png)

###### 导入Test PNG的Sphere测试图：

![image-20260423221931131](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423221931131.png)

###### 猜猜是哪个动漫角色图（sphere + 背景环境贴图）

![image-20260423222201159](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260423222201159.png)

整体宏观图

![image-20260424200133735](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424200133735.png)

角落细节图

![image-20260424195932209](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424195932209.png)

Cornell图

自发光，更多的材质球获得了周围环境的信息（上下左右颜色的面，外部的材质贴图）

![image-20260424201413562](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260424201413562.png)

