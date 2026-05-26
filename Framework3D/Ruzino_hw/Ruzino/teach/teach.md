# 作业文档

# 7. Rasterization and Path Tracing



> 作业步骤：
>
> - 查看[作业文档](https://github.com/USTC-CG/USTC_CG_26/blob/main/Homeworks/7_rasterization_and_path_tracing/docs/README.md)
> - 在[项目目录](https://github.com/USTC-CG/USTC_CG_26/blob/main/Framework3D)中编写作业代码
> - 按照[作业规范](https://github.com/USTC-CG/USTC_CG_26/blob/main/Homeworks/README.md)提交作业

## 作业递交



- 递交内容：程序代码、实验报告及 `stage.usdc` 文件，见[提交文件格式](https://github.com/USTC-CG/USTC_CG_26/tree/main/Homeworks/7_rasterization_and_path_tracing#提交文件格式)
- 递交时间：2026年4月26日（周日）晚

## 要求



- 基础任务

  - (Rast.) 实现 Blinn-Phong 着色模型 (

    参考资料

    )

    - 正确计算法线贴图
    - 正确计算着色公式

  - (Rast.) 实现 Shadow Mapping 算法 ([参考资料](https://learnopengl-cn.github.io/05 Advanced Lighting/03 Shadows/01 Shadow Mapping/))

  - (P.T.) 矩形光源相关内容 （相交计算，采样计算，Irradiance计算）

  - (P.T.) 路径追踪算法中着色递归计算与Russian Roulette

- 可选任务

  - (Rast. )实现 Percentage Close Soft Shadow ([参考资料](https://zhuanlan.zhihu.com/p/478472753))
  - (Rast.) 实现 Screen Space Ambient Occlusion ([参考资料](https://learnopengl-cn.github.io/05 Advanced Lighting/09 SSAO/#ssao))
  - (Rast.) 实现Displacement Mapping ([Ref](https://zhuanlan.zhihu.com/p/369442463))
  - (P.T.) 实现更复杂的BRDF模型并进行MIS ([Ref](https://zhuanlan.zhihu.com/p/379681777))

- 拓展任务

  - 全面对比光栅 & 光追效果、时间等效果
  - 修改参数，对比效果
  - 修改光栅光追渲染方法，实现非真实感渲染 [Ref](https://zhuanlan.zhihu.com/p/142145970)

## 提供的材料



### (1)说明文档



本次作业的要求说明和一些辅助资料

- 作业文档(今年的) [->](https://github.com/USTC-CG/USTC_CG_26/blob/main/Homeworks/7_rasterization_and_path_tracing/docs)
- 光栅化去年的文档(仅供参考) [->](https://github.com/USTC-CG/USTC_CG_26/blob/main/Homeworks/7_rasterization_and_path_tracing/docs-rast/README.md)
- 光追去年年的文档(仅供参考) [->](https://github.com/USTC-CG/USTC_CG_25/blob/main/Homeworks/7_path_tracing/rtfd.pdf)

### (2)作业框架 [->](https://github.com/USTC-CG/USTC_CG_26/blob/main/Framework3D)



### (3)测试数据 [->](https://github.com/USTC-CG/USTC_CG_26/blob/main/Homeworks/7_rasterization_and_path_tracing/data)



> 注：其中data_c中1.usda将**法线贴图**替换为了**位移贴图**，即你在shader中访问的法线贴图实际上为位移贴图！ data_a中数据建议在PT中使用！

## 提交文件格式



完成作业之后，打包三类内容即可：

- 修改的程序代码
  - `Ruzino\source\Plugins\hd_RUZINO_GL` 文件夹
  - `Ruzino\source\Plugins\hd_RUZINO_Embree` 文件夹
  - 其他你修改过的东西
- 报告：请**提交PDF**格式
- stage.usdc； 在Ruzino目录下Assets文件夹中

### 注意事项



- 导入数据（网格和纹理）时使用**相对路径**，例如，将你的数据放在**可执行文件目录**下，直接通过 `FilePath = 'xxx.usda'` 或者 `FilePath = 'zzz.png'` 访问，或者定位到作业目录的 `data/` 文件夹中
- 请大家**尽量将算法相关代码都在节点定义文件中写完**，**避免**去额外创建其他的头文件/源文件将算法分离出去
- 节点的**输入&输出你都是可以修改**的，不要死脑经说“我这个明明要xxxx输入/输出，为什么节点没有xxxxx”



# 实现方案分析
# 光栅化部分**分步实现方案（按作业要求+框架逻辑排序）**
严格按照**从基础数据→光照模型→阴影→可选效果**的逻辑，结合Ruzino框架的节点与shader修改要求，一步步完成，**先做基础必做，再做可选拓展**。

## 一、前置准备（必须第一步）
1. **更新Ruzino框架**
    按要求拉取最新main分支代码，更新submodule，保证框架是最新版（避免节点/shader缺失）。
2. **构建环境**
    用**Release模式**编译框架（作业明确要求，速度更快），确保可执行文件正常运行。
3. **准备测试场景**
    - 新建Mesh → 导入作业提供的USD测试模型（用相对路径）。
    - 场景中添加**Sphere Light**（光栅化仅支持此光源），调整位置/亮度，显示变换Gizmo。
    - 切换渲染器为**光栅化渲染器**，打开渲染节点编辑器。

## 二、基础任务1：搭建光栅化核心节点链路（必须第二步）
先把节点连对，才能看到渲染结果，作业明确给出节点依赖：
1. 核心节点连接顺序：
    `rasterize_impl` → `deferred_lighting` → `present_color`
2. 阴影节点接入：
    `shadow_mapping` 输出深度图 → 接入 `deferred_lighting` 的阴影输入
3. 确认节点：
    - `rasterize_impl`：输出G-Buffer（位置、法线、反照率、高光参数等）。
    - `deferred_lighting`：执行Blinn-Phong+阴影计算，输出最终颜色。
    - `present_color`：把最终颜色显示到屏幕。
4. 小技巧：修改shader后**直接保存**，框架自动热加载，不用重新编译。

## 三、基础任务2：实现G-Buffer法线贴图采样（必须第三步）
作业要求Blinn-Phong必须**正确计算法线贴图**，这是光照的基础：
1. **修改文件**：`rasterize_impl.fs`（渲染G-Buffer的片元着色器）。
2. 核心工作：
    - 采样模型的法线贴图纹理。
    - 把**切线空间法线**转换为**世界空间法线**。
    - 将正确的世界空间法线写入G-Buffer输出。
3. 目的：保证后续Blinn-Phong使用的法线是正确的（而非模型默认法线）。

## 四、基础任务3：实现Blinn-Phong着色模型（必须第四步）
作业核心基础任务，在延迟着色中完成光照计算：
1. **修改文件**：`blinn_phong.fs`（延迟渲染的片元着色器）。
2. 分步实现：
    1. 从G-Buffer读取数据：世界空间位置、世界空间法线、反照率、高光指数/强度。
    2. 计算光照向量：光源方向、视角方向。
    3. 实现Blinn-Phong公式：
       - 漫反射项：`diffuse = 漫反射系数 * max(dot(法线, 光源方向), 0)`
       - 高光项：`specular = 高光系数 * pow(max(dot(半程向量, 法线), 0), 高光指数)`
       - 最终光照：`环境光 + 漫反射 + 高光`
    4. 把光照结果输出为片段颜色。
3. 验证：此时场景应出现正确的Blinn-Phong光照效果（无阴影）。

## 五、基础任务4：实现Shadow Mapping阴影算法（必须第五步）
作业第二个基础任务，分**两步**完成：
### 子步骤1：生成阴影深度图
1. **修改文件**：`shadow_mapping.vs` + `shadow_mapping.fs`。
2. 核心工作：
    - 顶点着色器：把顶点变换到**光源空间**，输出光源空间深度。
    - 片元着色器：输出光源空间深度值，生成阴影深度图。
    - 保证`shadow_mapping`节点从光源视角渲染深度图（默认朝向(0,0,0)）。

### 子步骤2：阴影采样与遮蔽计算
1. **修改文件**：`blinn_phong.fs`。
2. 核心工作：
    - 把当前像素的世界位置转换到**光源空间**。
    - 采样阴影深度图，对比当前深度与深度图值。
    - 计算阴影因子：深度更大则为阴影（乘0），否则正常光照（乘1）。
    - 把阴影因子乘到Blinn-Phong光照结果上，实现硬阴影。
3. 验证：场景出现正确的硬阴影，光照+阴影效果完整。

---

## 六、可选任务（按优先级/难度排序，基础完成后再做）
### 可选1：实现PCSS（软阴影）
1. **修改文件**：`blinn_phong.fs`（在Shadow Mapping基础上扩展）。
2. 步骤：
    1. 计算阴影贴图的平均深度。
    2. 根据深度差计算软阴影的模糊半径。
    3. 对阴影深度图做**百分比过滤**，输出软阴影。
3. 效果：硬阴影变柔和，边缘过渡自然。

### 可选2：实现SSAO（屏幕空间环境光遮蔽）
1. **修改文件**：`ssao.fs`。
2. 步骤：
    1. 生成随机采样核+旋转噪声纹理。
    2. 从G-Buffer读取位置/法线，计算遮蔽因子。
    3. 对遮蔽结果做模糊降噪。
    4. 把SSAO因子乘到最终光照结果上。
3. 效果：物体缝隙/角落出现自然的暗部遮蔽。

### 可选3：实现Displacement Mapping（位移贴图）
1. **修改文件**：`rasterize_impl.vs`（顶点着色器）。
2. 步骤：
    1. 采样位移贴图（注意data_c中1.usda的法线贴图实际是位移贴图）。
    2. 根据位移值沿法线方向**偏移顶点位置**。
    3. 修正法线，保证光照正确。
3. 效果：模型表面产生凹凸的几何位移，而非仅法线贴图的视觉凹凸。

---

## 七、最终验证（全部完成后）
1. 检查：法线贴图正确、Blinn-Phong光照正常、Shadow Mapping阴影无错。
2. 测试：切换不同模型/光源参数，确认效果稳定。
3. 提交：保留`hd_RUZINO_GL`文件夹所有修改，配合stage.usdc+报告打包。

要不要我帮你整理**每一步对应的shader核心逻辑要点**（不用写代码，只讲思路），方便你直接对着写？

## 光线追踪部分

光追作业需要大家**修改渲染器内部代码**以实现相应功能。渲染器在

**Ruzino\source\Plugins\hd_RUZINO_Embree** 下。

代码**默认使用直接光照积分器，大家需要在****renderer.cpp****中对积分器进行切换**，可以根据

“TODO”找到相应位置（DirectLightIntegrator切换为PathIntegrator）

完成**基础作业需要修改的代码大部分我标注了****“TODO”****的，搜索一下就能搜得到**。强调一

下，这**只是****“****大部分****”****，不代表你需要修改的地方只有这些**。在hd_USTC_CG_Embree文件夹

下面所有东西你都是可以修改的！

如果你完成得对的话（基础内容），你可以得到类似这样的结果（场景中有Dome Light & 

Sphere Light，开启光源显示）

## 下一步实现选做任务

- (Rast. )实现 Percentage Close Soft Shadow ([参考资料](https://zhuanlan.zhihu.com/p/478472753))
- (Rast.) 实现 Screen Space Ambient Occlusion ([参考资料](https://learnopengl-cn.github.io/05 Advanced Lighting/09 SSAO/#ssao))
- (Rast.) 实现Displacement Mapping ([Ref](https://zhuanlan.zhihu.com/p/369442463))