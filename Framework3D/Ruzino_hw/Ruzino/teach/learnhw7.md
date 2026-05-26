

1. 核心定义
    .fs 是 Fragment Shader 的缩写，是 OpenGL/OpenGL ES/Vulkan 图形渲染管线中，专门编写片段着色器的纯文本源码文件（对应的顶点着色器通常用.vs后缀）。它的核心作用是：GPU 会对屏幕上每一个待渲染的像素（片段）并行执行该文件内的代码，最终决定这个像素的颜色、透明度、深度等渲染属性，是实现光照、纹理采样、颜色渐变、后期特效等渲染效果的核心。
2. 语法规范
    .fs 文件遵循 GLSL（OpenGL Shading Language） 语法，是类 C 语言、专为 GPU 并行计算设计的着色器语言，有专属的关键字、数据类型和渲染管线规则，核心语法要点如下：
    版本声明：文件开头必须指定 GLSL 版本，必须和项目的 OpenGL/Vulkan 版本匹配，例如 #version 450 core，否则会编译报错。
    输入输出关键字
    in：输入变量，接收顶点着色器传递、经 GPU 插值后的变量（如纹理坐标 uv、顶点颜色、法线方向），每个像素的输入值都是插值后的结果；
    out：输出变量，一般为最终的片元颜色，通常是vec4类型（对应 RGBA 四个通道）；
    老版本 GLSL（330 之前）会用varying关键字做顶点 - 片段着色器的变量传递。
    统一变量 uniform：CPU 端传递给 GPU 的全局只读变量，整个绘制调用中所有像素共享该值，比如纹理采样器sampler2D、变换矩阵、光照颜色、时间参数等。
    专属数据类型
    向量：vec2/vec3/vec4（2/3/4 维浮点向量）、ivec2/uvec2等整型向量；
    矩阵：mat2/mat3/mat4（2/3/4 维浮点矩阵，多用于坐标变换）；
    采样器：sampler2D（2D 纹理采样，最常用）、samplerCube（立方体贴图）等。
    内置变量：片段着色器专属内置变量，例如 gl_FragCoord（当前片段的屏幕坐标）、gl_FragDepth（自定义深度值），老版本 GLSL 可直接用gl_FragColor输出最终颜色。
    入口函数：必须包含void main()函数，GPU 对每个像素都会执行一次该函数，是着色器的执行入口。
    精度限定符：OpenGL ES / 移动端场景必须声明浮点精度，例如 precision mediump float;，用于平衡渲染性能和精度。

```
https://learnopengl-cn.github.io/02%20Lighting/03%20Materials/ 
```

# 材质

| 原文 | [Materials](http://learnopengl.com/#!Lighting/Materials) |
| :--- | :------------------------------------------------------- |
| 作者 | JoeyDeVries                                              |
| 翻译 | Krasjet                                                  |
| 校对 | [AoZhang](https://github.com/SuperAoao)                  |

在现实世界里，每个物体会对光产生不同的反应。比如，钢制物体看起来通常会比陶土花瓶更闪闪发光，一个木头箱子也不会与一个钢制箱子反射同样程度的光。有些物体反射光的时候不会有太多的散射(Scatter)，因而产生较小的高光点，而有些物体则会散射很多，产生一个有着更大半径的高光点。如果我们想要在OpenGL中模拟多种类型的物体，我们必须针对每种表面定义不同的材质(Material)属性。

在上一节中，我们定义了一个物体和光的颜色，并结合环境光与镜面强度分量，来决定物体的视觉输出。当描述一个表面时，我们可以分别为三个光照分量定义一个材质颜色(Material Color)：环境光照(Ambient Lighting)、漫反射光照(Diffuse Lighting)和镜面光照(Specular Lighting)。通过为每个分量指定一个颜色，我们就能够对表面的颜色输出有细粒度的控制了。现在，我们再添加一个反光度(Shininess)分量，结合上述的三个颜色，我们就有了全部所需的材质属性了：

```c++
#version 330 core
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
}; 

uniform Material material;
```

在片段着色器中，我们创建一个结构体(Struct)来储存物体的材质属性。我们也可以把它们储存为独立的uniform值，但是作为一个结构体来储存会更有条理一些。我们首先定义结构体的布局(Layout)，然后简单地以刚创建的结构体作为类型声明一个uniform变量。

如你所见，我们为风氏光照模型的每个分量都定义一个颜色向量。ambient材质向量定义了在环境光照下这个表面反射的是什么颜色，通常与表面的颜色相同。diffuse材质向量定义了在漫反射光照下表面的颜色。漫反射颜色（和环境光照一样）也被设置为我们期望的物体颜色。specular材质向量设置的是表面上镜面高光的颜色（或者甚至可能反映一个特定表面的颜色）。最后，shininess影响镜面高光的散射/半径。

有这4个元素定义一个物体的材质，我们能够模拟很多现实世界中的材质。[devernay.free.fr](http://devernay.free.fr/cours/opengl/materials.html)中的一个表格展示了一系列材质属性，它们模拟了现实世界中的真实材质。下图展示了几组现实世界的材质参数值对我们的立方体的影响：

![img](https://learnopengl-cn.github.io/img/02/03/materials_real_world.png)

可以看到，通过正确地指定一个物体的材质属性，我们对这个物体的感知也就不同了。效果非常明显，但是要想获得更真实的效果，我们需要以更复杂的形状替换这个立方体。在[模型加载](https://learnopengl-cn.github.io/03 Model Loading/01 Assimp/)章节中，我们会讨论更复杂的形状。

搞清楚一个物体正确的材质设定是个困难的工程，这主要需要实验和丰富的经验。用了不合适的材质而毁了物体的视觉质量是件经常发生的事。

让我们试着在着色器中实现这样的一个材质系统。

# 设置材质

我们在片段着色器中创建了一个材质结构体的uniform，所以下面我们希望修改一下光照的计算来遵从新的材质属性。由于所有材质变量都储存在一个结构体中，我们可以从uniform变量material中访问它们：

```c++
void main()
{    
    // 环境光
    vec3 ambient = lightColor * material.ambient;

    // 漫反射 
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = lightColor * (diff * material.diffuse);

    // 镜面光
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 specular = lightColor * (spec * material.specular);  

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}
```

可以看到，我们现在在需要的地方访问了材质结构体中的所有属性，并且这次是根据材质的颜色来计算最终的输出颜色的。物体的每个材质属性都乘上了它们各自对应的光照分量。

我们现在可以通过设置适当的uniform来设置应用中物体的材质了。GLSL中一个结构体在设置uniform时并无任何区别，结构体只是充当uniform变量们的一个命名空间。所以如果想填充这个结构体的话，我们必须设置每个单独的uniform，但要以结构体名为前缀：

```c++
lightingShader.setVec3("material.ambient",  1.0f, 0.5f, 0.31f);
lightingShader.setVec3("material.diffuse",  1.0f, 0.5f, 0.31f);
lightingShader.setVec3("material.specular", 0.5f, 0.5f, 0.5f);
lightingShader.setFloat("material.shininess", 32.0f);
```

我们将环境光和漫反射分量设置成我们想要让物体所拥有的颜色，而将镜面分量设置为一个中等亮度的颜色，我们不希望镜面分量过于强烈。我们仍将反光度保持为32。

现在我们能够轻松地在应用中影响物体的材质了。运行程序，你会得到像这样的结果：

![img](https://learnopengl-cn.github.io/img/02/03/materials_with_material.png)

不过看起来真的不太对劲？

## 光的属性

这个物体太亮了。物体过亮的原因是环境光、漫反射和镜面光这三个颜色对任何一个光源都全力反射。光源对环境光、漫反射和镜面光分量也分别具有不同的强度。前面的章节中，我们通过使用一个强度值改变环境光和镜面光强度的方式解决了这个问题。我们想做类似的事情，但是这次是要为每个光照分量分别指定一个强度向量。如果我们假设lightColor是`vec3(1.0)`，代码会看起来像这样：

```c++
vec3 ambient  = vec3(1.0) * material.ambient;
vec3 diffuse  = vec3(1.0) * (diff * material.diffuse);
vec3 specular = vec3(1.0) * (spec * material.specular);
```

所以物体的每个材质属性对每一个光照分量都返回了最大的强度。对单个光源来说，这些`vec3(1.0)`值同样可以对每种光源分别改变，而这通常就是我们想要的。现在，物体的环境光分量完全地影响了立方体的颜色，可是环境光分量实际上不应该对最终的颜色有这么大的影响，所以我们会将光源的环境光强度设置为一个小一点的值，从而限制环境光颜色：

```c++
vec3 ambient = vec3(0.1) * material.ambient;
```

我们可以用同样的方式影响光源的漫反射和镜面光强度。这和我们在[上一节](https://learnopengl-cn.github.io/02 Lighting/02 Basic Lighting/)中所做的极为相似，你可以认为我们已经创建了一些光照属性来影响各个光照分量。我们希望为光照属性创建类似材质结构体的东西：

```c++
struct Light {
    vec3 position;

    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform Light light;
```

一个光源对它的ambient、diffuse和specular光照分量有着不同的强度。环境光照通常被设置为一个比较低的强度，因为我们不希望环境光颜色太过主导。光源的漫反射分量通常被设置为我们希望光所具有的那个颜色，通常是一个比较明亮的白色。镜面光分量通常会保持为`vec3(1.0)`，以最大强度发光。注意我们也将光源的位置向量加入了结构体。

和材质uniform一样，我们需要更新片段着色器：

```c++
vec3 ambient  = light.ambient * material.ambient;
vec3 diffuse  = light.diffuse * (diff * material.diffuse);
vec3 specular = light.specular * (spec * material.specular);
```

我们接下来在应用中设置光照强度：

```c++
lightingShader.setVec3("light.ambient",  0.2f, 0.2f, 0.2f);
lightingShader.setVec3("light.diffuse",  0.5f, 0.5f, 0.5f); // 将光照调暗了一些以搭配场景
lightingShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f); 
```

现在我们已经调整了光照对物体材质的影响，我们得到了一个与上一节很相似的视觉效果。但这次我们有了对光照和物体材质的完全掌控：

![img](https://learnopengl-cn.github.io/img/02/03/materials_light.png)

改变物体的视觉效果现在变得相对容易了。让我们做点更有趣的事！

## 不同的光源颜色

到目前为止，我们都只对光源设置了从白到灰到黑范围内的颜色，这样只会改变物体各个分量的强度，而不是它的真正颜色。由于现在能够非常容易地访问光照的属性了，我们可以随着时间改变它们的颜色，从而获得一些非常有意思的效果。由于所有的东西都在片段着色器中配置好了，修改光源的颜色非常简单，并立刻创造一些很有趣的效果：

<video src="https://learnopengl-cn.github.io/img/02/03/materials.mp4" controls="controls" style="box-sizing: border-box; display: block; margin-left: auto; margin-right: auto; color: rgb(34, 34, 34); font-family: &quot;Microsoft Yahei&quot;, Lato, proxima-nova, &quot;Helvetica Neue&quot;, Arial, sans-serif; font-size: 15px; font-style: normal; font-variant-ligatures: normal; font-variant-caps: normal; font-weight: 400; letter-spacing: normal; orphans: 2; text-align: start; text-indent: 0px; text-transform: none; widows: 2; word-spacing: 0px; -webkit-text-stroke-width: 0px; white-space: normal; background-color: rgb(255, 255, 255); text-decoration-thickness: initial; text-decoration-style: initial; text-decoration-color: initial;"></video>

你可以看到，不同的光照颜色能够极大地影响物体的最终颜色输出。由于光照颜色能够直接影响物体能够反射的颜色（回想[颜色](https://learnopengl-cn.github.io/02 Lighting/01 Colors/)这一节），这对视觉输出有着显著的影响。

我们可以利用sin和glfwGetTime函数改变光源的环境光和漫反射颜色，从而很容易地让光源的颜色随着时间变化：

```c++
glm::vec3 lightColor;
lightColor.x = sin(glfwGetTime() * 2.0f);
lightColor.y = sin(glfwGetTime() * 0.7f);
lightColor.z = sin(glfwGetTime() * 1.3f);

glm::vec3 diffuseColor = lightColor   * glm::vec3(0.5f); // 降低影响
glm::vec3 ambientColor = diffuseColor * glm::vec3(0.2f); // 很低的影响

lightingShader.setVec3("light.ambient", ambientColor);
lightingShader.setVec3("light.diffuse", diffuseColor);
```

尝试并实验一些光照和材质值，看看它们是怎样影响视觉输出的。你可以在[这里](https://learnopengl.com/code_viewer_gh.php?code=src/2.lighting/3.1.materials/materials.cpp)找到应用的源码。

## 练习

- 你能做到这件事吗，改变光照颜色导致改变光源立方体的颜色？
- 你能像教程一开始那样，通过定义相应的材质来模拟现实世界的物体吗？注意[材质表格](http://devernay.free.fr/cours/opengl/materials.html)中的环境光值与漫反射值不一样，它们没有考虑光照的强度。要想正确地设置它们的值，你需要将所有的光照强度都设置为`vec3(1.0)`，这样才能得到一致的输出：[参考解答](https://learnopengl.com/code_viewer_gh.php?code=src/2.lighting/3.2.materials_exercise1/materials_exercise1.cpp)：青色塑料(Cyan Plastic)容器。



# 代码学习

1,G buffer法线部分：
## G-Buffer 生成 Pass 数学原理详解

---

### 1. 位置与透视深度计算
这部分将顶点从**世界空间**变换到**裁剪空间**，并计算透视除法后的归一化深度值。

#### 数学逻辑
1. **裁剪空间变换**：
   将世界空间顶点位置 $\boldsymbol{P}_{\text{world}}$ 依次通过视图矩阵 $\boldsymbol{V}$ 和投影矩阵 $\boldsymbol{P}$ 变换，得到裁剪空间坐标 $\boldsymbol{P}_{\text{clip}}$：
   $$
   \boldsymbol{P}_{\text{clip}} = \boldsymbol{P} \cdot \boldsymbol{V} \cdot \begin{pmatrix} \boldsymbol{P}_{\text{world}} \\ 1.0 \end{pmatrix}
   $$
   其中 $\boldsymbol{P}_{\text{world}}$ 对应代码中的 `vertexPosition`。

2. **透视深度计算**：
   对裁剪空间坐标执行**透视除法**（Perspective Division），得到归一化设备坐标（NDC），并提取深度分量：
   $$
   \text{depth} = \frac{\boldsymbol{P}_{\text{clip}}.z}{\boldsymbol{P}_{\text{clip}}.w}
   $$
   该深度值将用于后续的 Shadow Mapping 等算法。

---

### 2. 法线贴图解码
法线贴图存储的是**切线空间法线**，且为了能存储在纹理中，被压缩到 $[0, 1]$ 范围。这一步将其还原回 $[-1, 1]$ 的向量范围。

#### 数学逻辑
1. **纹理采样**：
   从法线贴图中采样得到原始值 $\boldsymbol{N}_{\text{tex}} \in [0, 1]^3$：
   $$
   \boldsymbol{N}_{\text{tex}} = \text{texture2D}(\text{normalMapSampler}, \text{vTexcoord}).xyz
   $$

2. **范围映射**：
   通过线性变换将 $[0, 1]$ 映射到 $[-1, 1]$，得到切线空间法线 $\boldsymbol{N}_{\text{tangent}}$：
   $$
   \boldsymbol{N}_{\text{tangent}} = \text{normalize}\left( 2.0 \cdot \boldsymbol{N}_{\text{tex}} - 1.0 \right)
   $$

---

### 3. 基于屏幕空间导数的 TBN 矩阵构建
这是代码的核心数学部分。TBN 矩阵用于将法线从**切线空间**变换到**世界空间**。由于模型未提供显式切线数据，代码使用**屏幕空间导数**（`dFdx`/`dFdy`）动态计算切线和副切线。

#### 3.1 屏幕空间导数的物理意义
在片元着色器中，`dFdx(val)` 表示 `val` 在屏幕空间 $x$ 方向（相邻像素）的变化率，`dFdy(val)` 表示在 $y$ 方向的变化率。

定义：
- 世界空间位置的屏幕导数：
  $$
  \boldsymbol{E}_1 = \text{dFdx}(\boldsymbol{P}_{\text{world}}), \quad \boldsymbol{E}_2 = \text{dFdy}(\boldsymbol{P}_{\text{world}})
  $$
- 纹理坐标的屏幕导数：
  $$
  \Delta \boldsymbol{UV}_1 = \text{dFdx}(\text{vTexcoord}), \quad \Delta \boldsymbol{UV}_2 = \text{dFdy}(\text{vTexcoord})
  $$

#### 3.2 切线向量的初始求解
切线 $\boldsymbol{T}$ 应满足：在纹理空间中沿 $u$ 方向延伸。通过线性方程组求解：
$$
\begin{cases}
\boldsymbol{E}_1 = \Delta \boldsymbol{UV}_1.x \cdot \boldsymbol{T} + \Delta \boldsymbol{UV}_1.y \cdot \boldsymbol{B} \\
\boldsymbol{E}_2 = \Delta \boldsymbol{UV}_2.x \cdot \boldsymbol{T} + \Delta \boldsymbol{UV}_2.y \cdot \boldsymbol{B}
\end{cases}
$$
消去副切线 $\boldsymbol{B}$，得到初始切线：
$$
\boldsymbol{T}_{\text{initial}} = \boldsymbol{E}_1 \cdot \Delta \boldsymbol{UV}_2.y - \boldsymbol{E}_2 \cdot \Delta \boldsymbol{UV}_1.y
$$

#### 3.3 鲁棒性处理与正交化
为避免数值不稳定和非正交问题，需进行**Gram-Schmidt 正交化**：

1. **退化情况处理**：
   若初始切线长度过小（$\|\boldsymbol{T}_{\text{initial}}\| < 10^{-7}$），通过副切线反推切线：
   $$
   \boldsymbol{B}_{\text{backup}} = -\boldsymbol{E}_1 \cdot \Delta \boldsymbol{UV}_2.x + \boldsymbol{E}_2 \cdot \Delta \boldsymbol{UV}_1.x \\
   \boldsymbol{T}_{\text{initial}} = \text{normalize}\left( \boldsymbol{B}_{\text{backup}} \times \boldsymbol{N}_{\text{vertex}} \right)
   $$
   其中 $\boldsymbol{N}_{\text{vertex}}$ 是顶点法线。

2. **Gram-Schmidt 正交化**：
   从初始切线中减去其在法线方向上的投影，确保切线与法线垂直：
   $$
   \boldsymbol{T} = \text{normalize}\left( \boldsymbol{T}_{\text{initial}} - \left( \boldsymbol{T}_{\text{initial}} \cdot \boldsymbol{N}_{\text{vertex}} \right) \cdot \boldsymbol{N}_{\text{vertex}} \right)
   $$

3. **副切线计算**：
   通过叉乘得到与切线、法线都垂直的副切线：
   $$
   \boldsymbol{B} = \text{normalize}\left( \boldsymbol{T} \times \boldsymbol{N}_{\text{vertex}} \right)
   $$

#### 3.4 TBN 矩阵构建与法线变换
构建 TBN 矩阵，将切线空间法线变换到世界空间：
$$
\text{TBN} = \begin{pmatrix} \boldsymbol{T} & \boldsymbol{B} & \boldsymbol{N}_{\text{vertex}} \end{pmatrix}
$$
最终世界空间法线为：
$$
\boldsymbol{N}_{\text{world}} = \text{normalize}\left( \text{TBN} \cdot \boldsymbol{N}_{\text{tangent}} \right)
$$


# 阴影映射相关

# 阴影映射

| 原文 | [Shadow Mapping](http://learnopengl.com/#!Advanced-Lighting/Shadows/Shadow-Mapping) |
| :--- | :----------------------------------------------------------- |
| 作者 | JoeyDeVries                                                  |
| 翻译 | [Django](http://bullteacher.com/)                            |
| 校对 | gjy_1992, [1i9h7_b1u3](https://github.com/1012796366/)       |

阴影是由于遮挡导致光线无法到达而形成的。当一个光源的光线因为被其他物体阻挡而无法照射到某个物体时，该物体便处于阴影之中。阴影为光照场景增添了极强的真实感，让观察者能够更容易感知物体之间的空间关系。为场景赋予了更强的立体感。例如，观察下方两张图，一张有阴影而另外一张没有阴影：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_with_without.png)

你可以看到，有阴影的时候你能更容易地区分出物体之间的位置关系，例如，只有在有阴影的情况下，我们才能明显看到其中一个立方体悬浮于其他立方体之上。

然而，实现阴影绝非易事，主要是因为目前的实时（光栅化图形）研究领域并没有开发出完美的阴影算法，虽然已经有许多优秀的阴影近似算法，但它们都有无法忽略的瑕疵。

大多数电子游戏中使用的一种技术是阴影映射(shadow mapping)，效果不错，而且相对容易实现。阴影映射并不难以理解，性能开销不算太高，而且非常容易扩展成更高级的算法（比如[全向阴影贴图](https://learnopengl-cn.github.io/05 Advanced Lighting/03 Shadows/02 Point Shadows/)和[级联阴影贴图](https://learnopengl-cn.github.io/08 Guest Articles/2021/01 CSM/)）。

## 阴影映射

阴影映射背后的思路非常简单：我们以光的位置为视角进行渲染，我们能看到的东西都将被点亮，看不见的一定是在阴影之中了。假设有一个地板，在光源和它之间有一个大盒子。由于从光源处向光线方向看去，可以看到这个盒子，但看不到地板的一部分，这部分就应该在阴影中了。

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_theory.png)

这里的所有蓝线代表光源可以看到的片段。黑线代表被遮挡的片段：它们会被渲染为处于阴影中的片段。如果我们绘制一条从光源出发，到达最右边盒子上的一个片段上的线段或射线(ray)，那么射线将先击中悬浮的盒子，随后才会到达最右侧的盒子。结果就是悬浮的盒子被照亮，而最右侧的盒子将处于阴影之中。

我们希望得到射线首次击中物体时的交点，然后用这个最近的点和射线上其他点进行对比。随后我们将测试一下，如果一个测试点比最近点更远的话，那么这个点就在阴影中。然而，若从这类光源发射出成千上万条光线并逐一遍历，是一种极为低效的方法，实时渲染上基本不可取。不过，我们可以采取相似举措，不用投射出光的射线，而是使用我们非常熟悉的东西：深度缓冲。

你可能记得在[深度测试](https://learnopengl-cn.github.io/04 Advanced OpenGL/01 Depth testing/)教程中，在深度缓冲里的一个值对应于片段在摄像机视角下的深度值，其范围在0到1之间。如果我们从光源的视角来渲染场景，并把生成的深度值储存到纹理中会怎样？通过这种方式，我们就能从光源的视角采样最近的深度值。最终便可获得该方向上第一个可见片段的深度值。我们将所有的深度值存储到一个纹理中，称之为深度贴图(depth map)或是阴影贴图(shadow map)。

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_theory_spaces.png)

左侧的图片展示了一个定向光源（所有光线都是平行的）在立方体下的表面投射的阴影。通过储存到深度贴图中的深度值，我们就能找到最近点，用来确定片段是否在阴影中。我们使用该光源的视图矩阵和投影矩阵，从光源的角度下渲染场景，从而生成深度贴图。这个投影矩阵和视图矩阵一同形成了一个变换矩阵*[Math Processing Error]*，它可以将任何三维位置转变到光源的可见坐标空间。

因为定向光源被设定为无限远，所以它没有具体的位置。然而，为了实现阴影映射，我们得从光源的某个虚拟位置，沿着定向光源方向来渲染场景。

我们可以看到，在右边的图中，平行光和观察者位置都与左图相同。我们渲染一个在点*[Math Processing Error]*处的片段，需要确定它是否在阴影中。我们得先使用变换矩阵*[Math Processing Error]*把点*[Math Processing Error]*变换到光源的坐标空间里。既然现在是从光的角度来看点*[Math Processing Error]*的，那么该点的z坐标就相当于它的深度值，本例中这个值是0.9。 通过点*[Math Processing Error]*的坐标，我们可以采样深度/阴影贴图，获得从光源视角中可见的最近深度值，结果是点*[Math Processing Error]*，本例中，最近的深度值是0.4。因为采样深度贴图的结果是一个小于点*[Math Processing Error]*的深度值，我们可以断定*[Math Processing Error]*被挡住了，它在阴影中了。

因此，阴影映射由两个步骤组成：首先，我们渲染深度贴图，然后我们像往常一样渲染场景，使用生成的深度贴图来计算片段是否在阴影之中。听起来有点复杂，但随着我们一步一步地讲解这个技术，就能理解了。

## 深度贴图

第一步我们需要生成一张深度贴图(Depth Map)。深度贴图是从光的透视图里渲染的深度纹理，用它计算阴影。因为我们需要将场景的渲染结果储存到一个纹理中，我们将再次需要帧缓冲。

首先，我们要为渲染的深度贴图创建一个帧缓冲对象：

```c++
unsigned int depthMapFBO;
glGenFramebuffers(1, &depthMapFBO);
```

然后，创建一个2D纹理，提供给帧缓冲的深度缓冲使用：

```c++
const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

unsigned int depthMap;
glGenTextures(1, &depthMap);
glBindTexture(GL_TEXTURE_2D, depthMap);
glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
             SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);  
```

生成深度贴图不太复杂。因为我们只关心深度值，我们要把纹理格式指定为GL_DEPTH_COMPONENT。我们还要把纹理的高宽设置为1024：这是深度贴图的分辨率。

把我们把生成的深度纹理作为帧缓冲的深度缓冲：

```c++
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
glDrawBuffer(GL_NONE);
glReadBuffer(GL_NONE);
glBindFramebuffer(GL_FRAMEBUFFER, 0);
```

我们只需要从光源角度渲染场景时的深度信息，因此不需要使用颜色缓冲。然而，不包含颜色缓冲的帧缓冲对象是不完整的，所以我们需要显式告诉OpenGL我们不会渲染任何颜色数据。我们用glDrawBuffer和glReadBuffer来把读和绘制缓冲设置为GL_NONE。

合理配置将深度值渲染到纹理的帧缓冲后，我们就可以开始第一步了：生成深度贴图。两个步骤的完整的渲染阶段，看起来有点像这样：

```c++
// 1. 首选渲染深度贴图
glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    ConfigureShaderAndMatrices();
    RenderScene();
glBindFramebuffer(GL_FRAMEBUFFER, 0);
// 2. 像往常一样渲染场景，但这次使用深度贴图
glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
ConfigureShaderAndMatrices();
glBindTexture(GL_TEXTURE_2D, depthMap);
RenderScene();
```

这段代码隐去了一些细节，但它表达了阴影映射的基本思路。这里一定要记得调用glViewport。因为阴影贴图经常和我们原来渲染的场景（通常是窗口分辨率）有着不同的分辨率，我们需要改变视口(viewport)的参数以适应阴影贴图的尺寸。如果我们忘了更新视口参数，最后的深度贴图要么太小要么就不完整。

### 光源空间的变换

前面那段代码中一个不清楚的函数是`ConfigureShaderAndMatrices`。在第二个步骤中，这和往常一样：确保投影矩阵和视图矩阵都已经正确设置，并且为每个物体设置相应的模型矩阵。然而，在第一个步骤中，我们需要使用不同的投影矩阵和视图矩阵来从光源角度渲染场景。

因为我们使用的是一个所有光线都平行的定向光。出于这个原因，我们将为光源使用正交投影矩阵，透视图将没有任何变形：

```c++
float near_plane = 1.0f, far_plane = 7.5f;
glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane);
```

上面的代码是本章演示场景中所使用的正交投影矩阵的示例。因为该投影矩阵间接决定可视区域的范围（例如哪些东西不会被裁切），所以你需要确保投影视锥的尺寸包括了应当出现在深度贴图里面的所有物体。当物体或者片段没有出现在深度贴图中的时候，它们就不会产生阴影。

为了创建一个视图矩阵来变换每个物体，把它们变换到从光源视角可见的空间中，我们将使用glm::lookAt函数；这次从光源的位置看向场景中央。

```c++
glm::mat4 lightView = glm::lookAt(glm::vec3(-2.0f, 4.0f, -1.0f), 
                                  glm::vec3( 0.0f, 0.0f,  0.0f), 
                                  glm::vec3( 0.0f, 1.0f,  0.0f));  
```

二者相结合为我们提供了一个光源空间的变换矩阵，它将每个世界空间向量变换到在光源位置可以看到的的空间；这正是我们渲染深度贴图所需要的。

```c++
glm::mat4 lightSpaceMatrix = lightProjection * lightView;
```

这个`lightSpaceMatrix`正是前面我们称为*[Math Processing Error]*的那个变换矩阵。有了`lightSpaceMatrix`，只要给每个着色器提供光源空间的投影矩阵和视图矩阵，我们就能像往常那样渲染场景了。然而，我们只关心深度值，并不执行复杂的（照明）片段计算。为了提升性能，我们将使用一个与之不同但更为简单的着色器来渲染出深度贴图。

### 渲染至深度贴图

当我们从光的角度来渲染场景的时候，我们会用一个比较简单的着色器，这个着色器只会把顶点变换到光空间。这个简单的着色器叫做`simpleDepthShader`，就是使用下面的这个着色器：

```c++
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}  
```

这个顶点着色器接收模型矩阵和顶点数据，使用`lightSpaceMatrix`变换到光源空间中。

由于我们没有颜色缓冲并且禁止了读取和绘制缓冲，因此生成的片段不需要进行任何处理，所以我们可以简单地使用一个空片段着色器：

```c++
#version 330 core

void main()
{             
    // gl_FragDepth = gl_FragCoord.z;
}
```

这个空片段着色器什么也不干，运行完后，深度缓冲会被更新。我们可以取消片段着色器中那一行的注释，来显式设置深度，但因为底层总会去设置深度缓冲，所以我们没必要显式设置，直接使用空的片段着色器即可。

现在，渲染深度/阴影贴图的过程如下所示：

```c++
simpleDepthShader.use();
glUniformMatrix4fv(lightSpaceMatrixLocation, 1, GL_FALSE, glm::value_ptr(lightSpaceMatrix));

glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    RenderScene(simpleDepthShader);
glBindFramebuffer(GL_FRAMEBUFFER, 0);  
```

这里的`RenderScene`函数接受着色器程序(shader program)作为参数，它调用所有相关的绘制函数，并在需要的地方设置相应的模型矩阵。

最终的成品是一个填充完整的深度缓冲区，其中存储了从光源视角可见的所有片段的最近深度值。通过将这个纹理渲染到一个2D四边形上（和我们在帧缓冲一节做的后期处理过程类似），就能在屏幕上显示出来下面的效果：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_depth_map.png)

我们使用下面的片段着色器来将深度贴图渲染到四边形上：

```c++
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;

void main()
{             
    float depthValue = texture(depthMap, TexCoords).r;
    FragColor = vec4(vec3(depthValue), 1.0);
}  
```

要注意的是当用透视投影矩阵而不是正交投影矩阵来显示深度时，存在一些细微的差异，因为使用透视投影时，深度是非线性的。本节教程的最后，我们会讨论这些不同之处。

你可以在[这里](https://learnopengl.com/code_viewer_gh.php?code=src/5.advanced_lighting/3.1.1.shadow_mapping_depth/shadow_mapping_depth.cpp)获得把场景渲染成深度贴图的源码。

## 渲染阴影

正确地生成深度贴图以后我们就可以开始生成阴影了。这段代码在片段着色器中执行，用来检验一个片段是否在阴影之中，不过我们在顶点着色器中进行光源空间的变换：

```c++
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main()
{    
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));
    vs_out.Normal = transpose(inverse(mat3(model))) * aNormal;
    vs_out.TexCoords = aTexCoords;
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
    gl_Position = projection * view * vec4(vs_out.FragPos, 1.0);
}
```

这段代码里的新内容是`FragPosLightSpace`这个输出向量。我们用和之前一样的`lightSpaceMatrix`（即生成深度贴图时，用于将世界空间中的顶点坐标变换到光源空间的矩阵），把世界空间中的顶点位置转换到光源空间，方便之后在片段着色器中使用。

我们用来渲染场景的主片段着色器使用了Blinn-Phong光照模型。在片段着色器中，我们计算阴影分量，当片段处于阴影中时，其值为1.0，当片段不在阴影中时，其值为0.0，然后将得到的漫反射分量和镜面分量乘以这个阴影分量。因为光散射的缘故，阴影很少是完全黑暗的，所以我们并没有让环境分量乘以阴影分量。

```c++
#version 330 core
out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    [...]
}

void main()
{           
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightColor = vec3(1.0);
    // 环境光
    vec3 ambient = 0.15 * lightColor;
    // 漫反射
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // 镜面高光
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;    
    // 计算阴影值
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);       
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;    

    FragColor = vec4(lighting, 1.0);
}
```

片段着色器大部分是从[高级光照](https://learnopengl-cn.github.io/05 Advanced Lighting/03 Shadows/01 Advanced Lighting.md/)教程中复制过来的，只不过加上了个阴影计算。我们声明一个`shadowCalculation`函数，用它计算阴影。片段着色器的最后，我们我们把漫反射分量和镜面分量乘以阴影分量的反值（1.0-`shadow`），这表示该片段有不受阴影遮挡的程度。这个片段着色器还需要两个额外输入，一个是变换到光源空间后的片段位置，另一个是第一个渲染阶段得到的深度贴图。

要检查一个片段是否在阴影中，首先要把光源空间片段位置转换为裁切空间的标准化设备坐标。当我们在顶点着色器输出一个裁切空间顶点位置到`gl_Position`时，OpenGL自动进行一个透视除法，例如，将裁切空间坐标的范围[-w,w]转为[-1,1]，这要将x、y、z分量除以向量的w分量来实现。由于裁切空间的`FragPosLightSpace`并不会通过`gl_Position`传到片段着色器里，我们必须自己做透视除法：

```c++
float ShadowCalculation(vec4 fragPosLightSpace)
{
    // 执行透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    [...]
}
```

这将返回片段在光源空间中的位置，并将范围限定在[-1,1]。

当使用正交投影矩阵时，向量的w分量仍保持不变，所以这一步实际上毫无意义。可是，当使用透视投影的时候，这一步又变得不可或缺。所以为了保证在两种投影矩阵下都能正常运作，就得留着这行。

因为来自深度贴图的深度范围是[0,1]，我们也打算使用`projCoords`从深度贴图中去采样，所以我们将NDC坐标范围变换为[0,1]。

```c++
projCoords = projCoords * 0.5 + 0.5;
```

译注

这里的意思是，上面的projCoords的xyz分量都是[-1,1]（下面会指出这对于远平面之类的点才成立），而为了和深度贴图的深度相比较，z分量需要变换到[0,1]；为了作为从深度贴图中采样的坐标，xy分量也需要变换到[0,1]。所以整个projCoords向量都需要变换到[0,1]范围。

通过此变换，投影坐标projCoords的[0,1]范围坐标将与第一个渲染阶段生成的NDC坐标精确对应。这样我们就能从深度贴图中获取光源视角下的最近深度值：

```c++
float closestDepth = texture(shadowMap, projCoords.xy).r;
```

为了得到片段的当前深度，我们简单获取投影向量的z坐标，它等于来自光的透视视角的片段的深度。

```c++
float currentDepth = projCoords.z;
```

实际的对比就是简单检查currentDepth是否高于closetDepth，如果是，那么片段就在阴影中。

```c++
float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;
```

完整的`shadowCalculation`函数是这样的：

```c++
float ShadowCalculation(vec4 fragPosLightSpace)
{
    // 执行透视除法
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // 变换到[0,1]的范围
    projCoords = projCoords * 0.5 + 0.5;
    // 取得最近点的深度(使用[0,1]范围下的fragPosLight当坐标)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 
    // 取得当前片段在光源视角下的深度
    float currentDepth = projCoords.z;
    // 检查当前片段是否在阴影中
    float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;

    return shadow;
}
```

激活这个着色器，绑定合适的纹理，激活第二个渲染阶段默认的投影矩阵以及视图矩阵，结果如下图所示：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_shadows.png)

如果你做对了，你会看到地板和上有立方体的阴影（尽管还是有不少瑕疵）。你可以从[这里](https://learnopengl.com/code_viewer_gh.php?code=src/5.advanced_lighting/3.1.2.shadow_mapping_base/shadow_mapping_base.cpp)找到demo程序的源码。

## 改进阴影映射

我们成功地让阴影映射工作起来了，但是你也看到了，由于若干（清晰可见的）与阴影映射相关的瑕疵，目前的效果还是不够完善，我们得做些修复，接下来的章节中我们将着重解决这些问题。

### 阴影失真

前面的图片中明显有不对的地方。放大看会发现明显的摩尔纹：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_acne.png)

我们可以看到地板四边形渲染出很大一块交替黑线。这种阴影的不真实感叫做阴影失真(Shadow Acne)，下图解释了成因：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_acne_diagram.png)

受阴影贴图的分辨率影响，当多个片段距离光源比较远的时候，它们可能从深度贴图中采样相同的深度值。图片中，每个斜坡代表深度贴图一个单独的纹理像素。你可以看到，多个片段会采样相同的深度值。

虽然很多时候没问题，但是当光源以某个角度照射表面的时候就会出问题，这种情况下深度贴图也是从这样的角度下进行渲染的，随后，多个片段就会从同一个斜坡的深度纹理像素中采样，其中一部分在地板上面，另一部分在地板下面；这样我们所得到的阴影就有了差异。也因此，一部分片段位于阴影中，而一部分则不是，最终产生了图片中的条纹样式。

我们可以用一个叫做阴影偏移（shadow bias）的技巧来解决这个问题，我们简单的对表面的深度（或深度贴图）应用一个偏移量，这样片段就不会被错误地认为在表面之下了。

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_acne_bias.png)

使用了偏移量后，所有采样点都获得了比表面深度更小的深度值，这样整个表面就正确地被照亮，没有任何阴影。我们可以这样实现这个偏移：

```c++
float bias = 0.005;
float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
```

一个0.005的偏移就能帮到很大的忙，但偏移值高度依赖于光源与表面的夹角，如果倾角特别大的话，那么阴影仍然还是会失真。更稳健的办法是根据表面朝向光线的角度来更改偏移量，使用点乘就可以实现这个办法：

```c++
float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
```

这里我们有一个偏移量的最大值0.05，和一个最小值0.005，它们是基于表面法线和光照方向的。这样像地板这样的表面几乎与光源垂直，得到的偏移就很小，而比如立方体的侧面这种表面得到的偏移就更大。下图展示了使用了阴影偏移后的同一个场景，可以看出效果的确更好：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_with_bias.png)

因为各个场景中合适的偏差值都不尽相同，所以可能需要经过一番调整后才能找到合适的偏移值，但大多情况下，实际上就是增加偏移量直到所有失真都被移除的问题。

### 阴影悬浮

使用阴影偏移的一个缺点是你对物体的实际深度应用了平移。偏移值足够大时，会导致阴影明显地偏离了实际物体，你可以从下图看到这个现象（这是一个夸张的偏移值）：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_peter_panning.png)

这个阴影失真叫做阴影悬浮(Peter Panning)，因为物体看起来轻轻悬浮在表面之上。

译注

Peter Pan就是长篇小说《彼得·潘》中的人物，panning有平移、悬浮之意，而且彼得潘恰好是个会飞的男孩。

我们可以使用一个叫技巧解决大部分的阴影悬浮问题：当渲染深度贴图时候使用正面剔除(front face culling)。你也许记得在[面剔除](https://learnopengl-cn.github.io/04 Advanced OpenGL/04 Face culling/)教程中OpenGL默认是背面剔除。而现在我们要告诉OpenGL，在渲染阴影贴图时要剔除正面。

因为我们只需要深度贴图的深度值，对于实心物体来说，无论我们用它们的正面还是背面都没问题。使用背面深度不会有错误，即使阴影在物体内部有错误，我们也看不见。

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_culling.png)

为了修复阴影悬浮，在阴影贴图生成阶段，我们要进行正面剔除，首先必须开启GL_CULL_FACE：

```c++
glCullFace(GL_FRONT);
RenderSceneToDepthMap();
glCullFace(GL_BACK); // 不要忘记设回原先的面剔除
```

这十分有效地解决了阴影悬浮的问题，但只适用于具有封闭内部空间的实心物体。在我们的场景中，该方法在立方体上工作的很好，但在地板上无效，这是因为地板是一个平面，因此将被完全剔除。如果打算使用这个技巧解决阴影悬浮，就应当只在合适的物体上进行正面剔除。

另外要注意的是，接近阴影面的物体仍然可能会出现不正确的效果。但一般来说，通过常规的偏移值调整就足以解决阴影偏移的问题了。

### 过采样

不论你是否喜欢，还有一个视觉差异，即光的视锥范围以外的区域也会被判定为处于阴影之中。这是因为当投影坐标超出光的视锥范围时，其值会比1.0大，此时采样的深度纹理就会超出他默认的范围[0,1]。根据纹理环绕方式，我们将会得到不正确的深度结果，它不是基于真实的来自光源的深度值。

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_outside_frustum.png)

如图所示，存在一个虚构的光照范围，超出该区域的部分就被阴影覆盖；这个区域实际上代表着深度贴图投影到地板上的范围。发生这种情况的原因是我们之前将深度贴图的环绕方式设置成了`GL_REPEAT`。

我们更希望让所有超出深度贴图范围的坐标，其深度值是1.0，这样超出的坐标将永远不在阴影之中（因为没有物体的深度值是大于1.0的）。我们可以配置一个纹理边界颜色，然后把深度贴图的纹理环绕选项设置为`GL_CLAMP_TO_BORDER`：

```c++
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
float borderColor[] = { 1.0, 1.0, 1.0, 1.0 };
glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
```

现在如果我们采样深度贴图[0,1]范围以外的区域，纹理函数总会返回一个1.0的深度值，由此得到的阴影值始终为0.0。结果看起来会更真实：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_clamp_edge.png)

图中仍有一部分是阴影区域，那部分区域的坐标超出了光的正交视锥的远平面。通过阴影方向可以看到，这片阴影区域总是出现在光源视锥的远平面外。

当一个点在光源空间的z坐标大于1.0时，表示该点超出了远平面。这种情况下，`GL_CLAMP_TO_BORDER`环绕方式不起作用，这是因为在进行深度比较时，z>1.0的结果始终为真值。

解决这个问题也很简单，只要投影向量的z坐标大于1.0，我们就把shadow的值强制设为0.0：

```c++
float ShadowCalculation(vec4 fragPosLightSpace)
{
    [...]
    if(projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}
```

通过边界颜色钳制并进行远平面特殊处理，我们最终解决了深度贴图的过采样问题。最终得到了预期之内的结果了：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_over_sampling_fixed.png)

这样的做法意味着阴影只会出现在深度贴图的覆盖范围内，而光源视锥体以外的区域不会产生阴影。在游戏开发过程中，不产生阴影的部分通常只会出现在远方，相比于此前让远方漆黑一片的做法，这种处理更合理一些。

## PCF

当前的阴影已经为场景增色不少，但是仍未达到理想水平。如果你放大来看，会发现分辨率对阴影映射造成了非常明显的影响。

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_zoom.png)

因为深度贴图的分辨率固定，一个纹理像素可能覆盖了多个片段，结果就是多个片段会从深度贴图中采样相同的深度值，并得到相同的阴影判定结果，这也就导致了图中的锯齿边缘。

你可以通过增加深度贴图的分辨率的方式来减少锯齿块，也可以尝试尽可能的让光的视锥贴合场景。

另一种（部分）解决方案叫做PCF，译作百分比渐进滤波(percentage-closer filtering)，这个术语涵盖了多种滤波函数，能生成更柔和的阴影，减少锯齿块。其核心思想是多次采样深度贴图，每一次采样的纹理坐标都稍有不同，独立判断每个采样点的阴影状态后，将子结果混合取平均，最终获得相对柔和的阴影。

一种简单的PCF实现是采样深度贴图周边纹理像素，并取平均值：

```c++
float shadow = 0.0;
vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
for(int x = -1; x <= 1; ++x)
{
    for(int y = -1; y <= 1; ++y)
    {
        float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
        shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
    }    
}
shadow /= 9.0;
```

此处的`textureSize`返回指定采样器纹理在0级mipmap的宽高向量，类型为vec2。取其倒数，即可得到单一一个纹理像素的大小，可以用来对纹理坐标进行偏移，从而确保每次都采样不同的深度值。本示例在每个投影坐标周围采样9个点来进行阴影判断，最终取平均值。

增加采样次数，同时/或是更改`texelSize`变量，可以进一步提升柔化效果。下图展示了应用简易PCF后的阴影：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_soft_shadows.png)

从稍微远一点的距离看去，阴影效果好多了，也不那么生硬了。如果你放大，仍会看到阴影的不真实感，但通常对于大多数应用来说效果已经很好了。

你可以从[这里](https://learnopengl.com/code_viewer_gh.php?code=src/5.advanced_lighting/3.1.3.shadow_mapping/shadow_mapping.cpp)找到这个例子的全部源码。

实际上PCF技术体系包含了更多的柔化阴影边缘的方案，但限于本章篇幅，我们将留在以后讨论。

### 正交投影 vs 透视投影

在渲染深度贴图的时候，正交(orthographic)矩阵和透视(perspective)矩阵之间存在本质差异。正交投影矩阵并不会将场景用透视图进行变形，所有视线/光线都是平行的，这使它对于定向光来说是个很好的投影矩阵。然而透视投影矩阵，会将所有顶点根据透视关系进行变形，结果因此而不同。下图展示了两种投影方式所产生的不同阴影区域：

![img](https://learnopengl-cn.github.io/img/05/03/01/shadow_mapping_projection.png)

透视投影对于光源来说更合理，不像定向光，它是有自己的位置的。透视投影因此更经常用在点光源和聚光灯上，而正交投影经常用在定向光上。

另一个细微差别是，透视投影矩阵，将深度缓冲视觉化经常会得到一个几乎全白的结果。这个是因为透视投影将深度值转换为非线性值，其变化范围大多集中在近平面附近。为了可以像使用正交投影一样合适地观察深度值，你必须先将非线性深度值转变为线性值，如我们在[深度测试](https://learnopengl-cn.github.io/04 Advanced OpenGL/01 Depth testing/)教程中已经讨论过的那样。

```c++
#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D depthMap;
uniform float near_plane;
uniform float far_plane;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // 转换为 NDC
    return (2.0 * near_plane * far_plane) / (far_plane + near_plane - z * (far_plane - near_plane));
}

void main()
{             
    float depthValue = texture(depthMap, TexCoords).r;
    FragColor = vec4(vec3(LinearizeDepth(depthValue) / far_plane), 1.0); // 透视
    // FragColor = vec4(vec3(depthValue), 1.0); // 正交
}  
```

这个深度值与我们见到的正交投影的深度值很相似。需要注意的是，这个只适用于调试；正交或投影矩阵的深度检查仍然保持原样，因为相关的深度并没有改变。

## BRDF 光追选做

# 图形学｜Robust：Multiple Importance Sampling 多重重要性采样



看VCM的时候用到了MIS，那么就根据递归学习法，先做一篇Robust上关于MIS的笔记，仅供回顾。
**概要**案例：[面光源](https://zhida.zhihu.com/search?content_id=172504232&content_type=Article&match_order=1&q=面光源&zhida_source=entity)的高光The multi-sample model [多重采样模型](https://zhida.zhihu.com/search?content_id=172504232&content_type=Article&match_order=1&q=多重采样模型&zhida_source=entity)The balance heuristic
\1. 案例：面光源的高光这里通过一个经典案例来阐述MIS技术，即**面光源**对**光泽(glossy)表面**的**高光**效果。通常来说计算这种高光有两种方法，第一种是对**光源采样**，第二种是对**BSDF采样**，这两种方法在各自擅长的情况下有不错的表现，而MIS则能在所有情况下都有不错的表现。1.1 The glossy highlights problem 光泽高光问题对如下图所示的一个面光源 对光泽表面 的高光，计算其射向眼睛的出射光线 ：![img](https://pic4.zhimg.com/v2-bd487ac94321661e93d3eb69dc46ebad_1440w.jpg)根据反射方程： 表示面光源 的入射光，并引入一个 作为表面粗糙度参数，值为1表示完全漫反射，值为0表示完全镜面反射。1.2 Two sampling strategies 两种采样策略**1.2.1 Sampling the BSDF 采样BSDF**关于BSDF采样就是根据PDF随机采样入射光，PDF尽可能正比于BSDF分布。恰好采样到光源 的入射光对高光有贡献，而没有采样到 的则没有贡献。**1.2.2 Sampling the light source 采样光源**因为BSDF随机采样存在大量的计算量浪费，所以直接对光源进行采样更加高效，即将积分域转换到光源上。1.3 Comparing the two strategies 比较两种策略两种策略效果如下图所示，由左到右光源尺寸变大，从上到下表面粗糙度变大。![img](https://pic3.zhimg.com/v2-2da360b5475b95d4fa12e6e1d3828f64_1440w.jpg)左图：BSDF采样 右图：光源采样显然，对于**小光源**来说，对BSDF采样的随机性太大，而未命中光源的部分都是浪费的采样光线，尤其对于BSDF很分散的粗糙表面(左下角)，所以得到的结果不如对光源采样。而对于**大光源**来说，此时虽然对光源采样了，但是BSDF在该方向的值很小或没有，尤其是BSDF很集中的光滑表面(右上角)，所以光滑表面表现得不如对BSDF采样。这些大方差产生的**原因**，是反射方程中的被积函数是BSDF项 和入射光项 的点乘，只有当我们采用完全正比于这个点乘的pdf的时候才能得到一个无方差的结果。而BSDF方法则是尽可能正比于BSDF项，忽略了入射光项；光源采样方法尽量正比于入射光项，忽略了BSDF项，因此都**无法考虑所有的被积函数项而在某些特定情况表现欠佳**。因此，这两种方法都可以认为是**重要性采样**方法，目的是近似被积函数的某个项，当另外的项在这个函数中占据主导的情况则会表现不佳。但是对于获取整个被积函数的近似函数是不现实的，因此引出了结合不同近似函数的多重方法，其中每一个近似函数尽可能正比于被积函数的一个项，从而在各种情况下得到一个低方差的结果，即**[多重重要性采样](https://zhida.zhihu.com/search?content_id=172504232&content_type=Article&match_order=1&q=多重重要性采样&zhida_source=entity)**。
\2. The multi-sample model 多重采样模型*对于MIS方法，首先要介绍**多重采样模型**(multi-sample model)，这种模型能够将**组合采样点**的任何无偏方法表示为一组的**权重函数**，即可以结合任意采样方法，而得到一个统一的表达式。对于积分式：在其积分域 内，我们可以给出 n 个不同的**采样分布**，其**pdf**标识为 ，每一个分布对于任意的 ， 和 都存在且能被计算。对于不同采样策略的分布 ，其采样点个数表示为 ，则采样点总数 。对于第 种分布的第 个采样点表示为 ，则 ，每个采样点独立分布。2.1 The multi-sample estimator 多重采样估计器这节内容简述了多重估计器的形式。考虑**对于每一个采样点都有不同权重**的估计器，其权重 由 决定，这个多重采样估计器表达式为：要想这个估计器是无偏的，则权重函数 要满足两个条件，首先当 的时候 ，其次当 的时候 ，证明略。2.2 Examples of weighting functions 权重函数示例**假设对于三种采样策略 各自取一个采样点，即 ，先假设权重函数是一个常量，则估计器表示为：其中 。这种结合策略存在缺陷，即其误差取决于最差情况，其中任一个策略的高方差会导致整体估计器的高方差，因为：另一种采样策略是无重复地划分积分域，即：再在每一个积分域 中采取一种采样策略 。此时的权重函数为当 的时候 ，否则等于0。这种方法在图形学中很常用，例如将开头案例中的积分域，划分为光源范围和非光源范围，再分别使用对光源采样和对BSDF采样或其他方法。另一种组合方法是将被积函数拆分成几项的和，即 ，例如将BSDF拆分成diffuse,glossy,specular三个部分，然后对每一个部分使用一种采样策略。2.3 Generality of the multi-sample model 一般性多重采样模型其一般性模型可以表示为如下形式，这个式子可以理解为多重、采样两个求和式：其中 称为 的采样贡献，其函数可以任意选定，但必须满足估计器 的无偏条件：当 的时候，这个一般性模型就成了2.2.1中的原始模型了。
\3. The balance heuristic3.1 Balance heuristic 方法第二节内容简述了探索各种无偏估计器组合成多重采样模型的方式，现在要找一个 来最小化估计器的方差。这里介绍一种优秀的权重函数，即**balance heuristic**。其函数表达式为：这种权重函数很容易实现，只需要**对于每个采样点计算其在每一个分布中的值**，一共要计算 k 个分布，且这些额外的开销并不算大。将这个权重函数代入到2.1节的估计器表达式可以得：Balance heuristic方法的伪代码为：![img](https://pic3.zhimg.com/v2-17baddf1a83321ccef5c348b4a70fa34_1440w.jpg)3.2 Improved combination strategies 优化方法对于原本采样策略存在大方差的情况，balance heuristic能得到一个不错的结果，但是对于一些原本就低方差的采样策略，使用balance heuristic的多重策略反而会增加方差，所以需要考虑在**低方差问题上优化balance heuristic性能**。优化方法是**锐化(sharpen)调整权重函数**，即让接近0的权重进一步减少，让接近1的权重进一步增加。对此存在的两种优化方法，称为**cutoff heurisitc**和**[power heuristic](https://zhida.zhihu.com/search?content_id=172504232&content_type=Article&match_order=1&q=power+heuristic&zhida_source=entity)**，且balance heuristic可以看作是这些方法的特殊情况(极限值)。3.2.1 The [cutoff heuristic](https://zhida.zhihu.com/search?content_id=172504232&content_type=Article&match_order=1&q=cutoff+heuristic&zhida_source=entity)这种方法通过放弃低权重采样点来实现优化，设cutoff阈值 ，则：其中 。其cutoff阈值决定了权重值抛弃的程度，当 的时候就是balance heuristic。3.2.2 The power heuristic这种方法则通过一个指数项来实现优化：作者发现 的时候能够得到一个不错的结果，因为在这种情况下 是正比于 的，当 的时候就是balance heuristic。以下是四种heuristic方法得到的关于第一节案例的误差比较，maximum heuristic是指 或 的极限情况，是不同于balance heuristic的另一个极限。



### 概括

### 一、采样策略：从 “单一重要性采样” 扩展为 “多策略并行采样”

原有光追模型通常仅实现**单一重要性采样策略**（要么对 BSDF 采样、要么对光源采样），而 MIS 任务需新增：

1. 双策略采样逻辑

   ：同时实现两种核心采样策略的完整流程：

   - **BSDF 采样分支**：按 BSDF 的 PDF 分布随机采样入射光方向（尽可能正比于 BSDF 项f(ω)），保留采样点的方向、PDF 值；
   - **光源采样分支**：直接对光源积分域采样（尽可能正比于入射光项Li(ω)），转换积分域后生成采样点，保留方向、PDF 值；

   

2. **采样点管理**：为每种策略维护独立的采样点计数（文档中ni），总采样点N=∑ni，并为每个采样点标记 “所属采样策略”（如`enum SampleType { BSDF_SAMPLE, LIGHT_SAMPLE }`）。

### 二、核心机制：新增 “无偏权重函数” 的实现框架

这是 MIS 最核心的新增模块，原有光追无权重分配逻辑，需新增：

1. 无偏权重的约束实现

   ：

   - 保证权重函数wi(x)满足无偏性：若采样点x非第i种策略生成，则wi(x)=0；对任意x，所有策略的权重和∑i=1nwi(x)=1；

   

2. Balance Heuristic 基础权重计算

   ：

   

   实现核心公式：

   

   wi(x)=∑k=1nnkpk(x)nipi(x)

   

   代码层面需新增函数（如

   ```
   float computeBalanceWeight(SamplePoint point, int n_i, float p_i, vector<int> all_n_k, vector<float> all_p_k)
   ```

   ），对每个采样点计算其在所有策略下的 PDF 值，再结合各策略采样数加权；

3. （可选）启发式优化权重

   ：

   - Cutoff Heuristic：新增阈值ϵ，抛弃低权重采样点（wi′(x)=max(0,wi(x)−ϵ)/(1−nϵ)）；
   - Power Heuristic：新增指数β（通常取 2），实现wi(x)=∑k=1nnkpk(x)βnipi(x)β。

   

### 三、积分估计：重构反射方程的积分计算逻辑

原有光追的反射方程积分是单一采样策略的无偏估计（I^=N1∑p(x)f(x)），需重构为：

1. 多重采样估计器实现

   ：

   

   实现文档中核心估计器公式：

   

   I^=∑i=1n∑j=1nipi(xij)wi(xij)f(xij)

   

   代码层面需遍历所有采样策略的所有采样点，对每个采样点计算 “权重 ×BRDF / 光源贡献 ÷ 对应 PDF”，再累加求和；

2. **积分域适配**：针对 “面光源 + 光泽表面高光” 场景，适配两种采样策略的积分域转换（如光源采样时将积分域从 “方向域” 转为 “光源表面域”）。

### 四、场景适配：针对 “面光源 - 光泽表面高光” 的特殊逻辑

新增对该核心场景的适配逻辑：

1. PDF 的场景化计算

   ：

   - BSDF 采样：结合表面粗糙度参数α（α=1漫反射、α=0镜面反射）计算 PDF；
   - 光源采样：结合面光源尺寸计算 PDF（大 / 小光源的 PDF 分布差异）；

   

2. **策略互补性适配**：无需人工切换采样策略，由权重函数自动平衡 —— 小光源 / 粗糙表面时光源采样权重更高，大光源 / 光滑表面时 BSDF 采样权重更高。

### 五、方差控制：新增低方差优化逻辑

原有光追单一采样策略在极端场景（如小光源用 BSDF 采样、大光源用光源采样）会出现高方差（噪点），需新增：

1. **方差自动平衡逻辑**：通过 MIS 权重函数让被积函数（BSDF 项 × 入射光项）的近似更贴合整体分布，而非仅贴合单一项；
2. **（可选）启发式方差调优**：Cutoff/Power Heuristic 进一步降低 “低方差场景下 MIS 额外引入的方差”（如原本低方差的采样策略，通过锐化权重减少其他策略的干扰）。

### 代码实现视角的核心新增模块总结

表格







| 模块类型 |        原有光追模型         |                       MIS 选做新增内容                       |
| :------: | :-------------------------: | :----------------------------------------------------------: |
| 数据结构 |   单一采样点（方向、PDF）   | 采样点扩展（所属策略、各策略 PDF 值）；采样策略枚举（BSDF/LIGHT） |
| 核心函数 | 单一采样生成、单一 PDF 计算 | 多策略采样生成函数；Balance/Power/Cutoff 权重计算函数；重构后的积分估计函数 |
| 逻辑流程 |     单策略采样→积分计算     |     多策略并行采样→全策略 PDF 计算→权重分配→加权积分求和     |
| 场景适配 |        通用光追场景         |    面光源 - 光泽表面高光的 PDF 适配；方差平衡的场景化优化    |

简言之，MIS 选做的核心是**通过 “多策略采样 + 无偏权重加权”，解决单一重要性采样在极端场景下的高方差问题**，所有新增内容均围绕 “多种采样策略的无偏组合” 展开，最终实现全场景下反射方程积分的低方差近似。



## 选做参考资料

# 实时阴影(二) CSM, PCSS与SDF Soft Shadow

[![陈陈](https://picx.zhimg.com/v2-ff6984ae39efa17a01fbf079a28a1e7c_l.jpg?source=32738c0c&needBackground=1)](https://www.zhihu.com/people/zhao-jun-44-85-42)

[陈陈](https://www.zhihu.com/people/zhao-jun-44-85-42)

关注他

106 人赞同了该文章







目录

收起

CSM

PCF:

PCSS:

SDF Soft Shadows:

参考资料：

[![img](https://pica.zhimg.com/v2-97960f480d37f57f408ca72cb18209a5.jpg?source=7e7ef6e2&needBackground=1)朝君：实时阴影(一) ShadowMap, PCF与Face Culling65 赞同 · 5 评论 ](https://zhuanlan.zhihu.com/p/477330771)文章[![img](https://pic1.zhimg.com/v2-74725cf872dd9bebfdd2c539445cd3ea.jpg?source=7e7ef6e2&needBackground=1)朝君：实时阴影(三) VSM与VSSM81 赞同 · 4 评论 ](https://zhuanlan.zhihu.com/p/483674565)文章上篇文章讲了最基础的阴影生成方式Shadow Map，也提到了它存在的不足和局限性。本篇来介绍一下在Shadow Map基础上进行优化的几个方法。由于Shadow Map分辨率有限，并且相机视角渲染的分辨率远大于Shadow Map的分辨率，导致在这种情况下我们能看到很明显的阴影锯齿。解决这个问题有两种思路，一是在阴影生成时使用更好的方法，二是在阴影测试时采用不同的采样方式。CSMCSM全称是Cascaded Shadow Maps，也就是级联的阴影贴图。它属于上述的第一个方法，即针对Shadow Map生成的改进。理想情况下只要阴影贴图的分辨率够大，锯齿的问题就能大大缓解，但是考虑到GPU的显存的负载问题，显然不能无限的增加分辨率。并且比起盲目的增加分辨率，通过在渲染画面中的观察，我们能发现：1.相比起近处的画面，远端的场景没有那么重要，我们通常不太注意远处的阴影质量。2.经过透视投影后，对于近处的物体，相邻两个像素对应在场景中的空间位置可能相差不大；而对于远处的物体，相邻的两个像素在世界坐标系下的位置相差许多，也就是说，对于远处的物体，本身就不容易产生“多个相邻像素对应到Shadow Map的同一个texel上而导致锯齿”这种问题。因此，早期的图形工作者相处了一种巧妙的方法：既然远处的物体不重要，可以用一张低分辨率的Shadow Map来保存。即采用多张（级联）的Shadow Map来保存场景中不同位置的阴影。CSM方法其实就是为将近处和远处的物体渲染到不同分辨率的ShadowMap上去，并根据片段着色器中片段的深度来选择ShadowMap的层级再对其进行采样。![img](https://pic4.zhimg.com/v2-adf6c3c87b2c54db636e8d759445121f_1440w.jpg)图1：CSGO中低配画质中的阴影，红圈处为切换阴影层级时带来的突变。[LearnOpenGL](https://link.zhihu.com/?target=https%3A//learnopengl.com/Guest-Articles/2021/CSM)中提供了一种基础的CSM算法：首先我们可以使用相机的视图和投影矩阵，从NDC反向计算出它所定义的frustum在世界坐标中的位置，然后将frustum划分为 n 个子视锥体，其中第 i 个截锥体的远平面是第 (i+1) 个截锥体的近平面。对于每一个子视锥体，计算出它的中心坐标，以此作为光源的坐标并计算出光源的view矩阵。将子视锥体使用光源的view矩阵变换到光源坐标空间中，选择合适的正交投影矩阵，并将结果渲染到不同分辨率的ShadowMap上。![img](https://pic4.zhimg.com/v2-b6a6db1c0adea38a07ac4a0bcea12b2b_1440w.jpg)上述的步骤十分简单，但过程中有许多细节值得推敲：1.如何划分子视锥体及如何选择合适的分辨率LearnOpenGL中虽然没有明确给出如何划分frustum与不同层级的ShadowMap的分辨率的差异，但从文章的内容可以看出作者只是将frustum按照深度进行简单的等分，ShadowMap的分辨率也是每层减小1/4（长宽各1/2），然而这种方式并不是十分合理。CSM最初想要解决的问题是想要通过增大分辨率来减小锯齿问题，并舍弃一部分远处物体的阴影贴图的精度来减少显存占用。对于第一级的ShadowMap，可以使用高一点的分辨率来保存，而对于后续的几级，我们希望它能够在**保证阴影质量不变**的前提下，尽可能的减小分辨率，而这个阴影质量可以通过shadow map aliasing error来衡量。![img](https://pic4.zhimg.com/v2-e5dd07421b2d82d5d96901cc531bd3d5_1440w.jpg)Parallel-Split Shadow Maps for Large-scale Virtual Environments上图蓝色的框表示frustum的侧视图，图中 表示第i个子frustum， 和 为改frustum的近远平面。位于 处的黄色物体在相机frustum上的投影长度为 ，在Shadow Map上的投影长度为 ，其本身在 轴上的长度为 ，经过简单的推导可得：上述的 就是我们希望保持不变的shadow map aliasing error。也就是说，我们希望处于不同深度的物体，它的每个像素在Shadow Map上的覆盖区域保持不变，让锯齿情况在场景不同深度下保持恒定。即代表perspective aliasing这一部分的 是个常数 :这里解释一下，所谓perspective aliasing表示的是由透视引起的锯齿。与之相对的另一部分 叫做projection aliasing，是由物体本身的几何决定的，在设计阴影的时候无法掌控，因此在计算中忽略。projection aliasing可以简单的理解为由于物体摆放的角度不同，造成它在Shadow Map上覆盖大小的不同。以上述黄色物体为例，当完全竖直放着的时候在Shadow Map上覆盖的区域最小，在进行深度测试和其上的所有像素都对应与ShadowMap的同一点；而在其横放是在ShadowMap上覆盖区域最大，最不容易出现锯齿。由于 范围是 ，可以得知 ，因此理论上ShadowMap纹理坐标与物体深度的关系为：考虑到实际的纹理是离散的，无法用这种连续的方式表达，因此对于 处第一种表示的是先确定划分位置后，计算这一段深度对应的ShadowMap的分辨率，第二种表示的是先确定分辨率(用多少个ShadowMap)再计算他们对应的深度值。在实际计算中两者皆可。以这种方式进行划分其实并不实用，上述的积分其实隐含这一个假设： 为物体在ShadowMap上的纹理坐标，其积分范围是 ，z为物体在世界坐标下的深度，其积分范围为 ，然而上述换元时做的积分域替换，只有在两者对应的情况下才成立，也就是当阴影贴图准确地覆盖了视锥体，并且没有任何分辨率浪费在场景的不可见部分上才可以（即图中灯光完全垂直于Z轴）。然而实际场景中并不一定都是如此，因此该篇论文的作者做了一个融合：![img](https://pica.zhimg.com/v2-db3b0a3049cf0a6ade3d21d4b0540e70_1440w.jpg)以上述方法划分的叫做logarithmic划分，另一种划分方式便是简单粗暴的等分(Uniform)，将两者做个简单的平均即可得到一个在实际场景中更合适的划分方式（最后一项 只是用来在场景中根据实际情况做微调）：2.如何选择正交投影矩阵同样的在LearnOpenGL中只是给出了一种简单的方式，对于每一个子视锥体的八个顶点，计算他们在空间中的包围盒，并以此作为投影矩阵的参数，这样尽可能的让ShadowMap覆盖可视区域。然而需要考虑到，在frustum中的物体仅仅是相机的可视区域，在其外仍有可能会有其他物体并将阴影投在可视区域中。因此对于frustum的xy-boundingbox可以简单的取其最大最小值，而z轴上的坐标需要考虑场景中的其他物体（xyz为光源空间下）。![img](https://pic2.zhimg.com/v2-c7108831a33744083819d7615c5bfe57_1440w.jpg)Nvidia Cascaded Shadow Maps3.重叠部分如何选择从上图或是LearnOpenGL中的图中都可以看出，在某些深度上的物体会被两个层级的ShadowMap所覆盖。这种情况下，一般有限考虑在分辨率更大的ShadowMap中进行采样，以获得更好的阴影质量。但是在临近边界时，需要混合一下两种采样的结果，以免出现开头那张图中阴影突变的情况。最后生成的ShadowMap类似于这样：![img](https://pic3.zhimg.com/v2-785b53d705ae2fc2c58d6475ecee8b98_1440w.jpg)最后在片段着色器中根据片段的深度选择合适的ShadowMap层级去采样就可以了。PCF:PCF全称是Percentage Closer Filtering，它所针对的是上文说到的第二种办法：优化阴影测试时的采样方式。阴影锯齿的原因和之前光栅化渲染的原因一样，本质上都是由于采样率不足，导致无法从原先的信号中获取足量的信息。因此很容易想到将抗锯齿的方法用到阴影锯齿上来。同样的，我们无法通过在一个已经有锯齿的图像上做一次滤波来获得更加平滑的结果，拿相机视角渲染的锯齿为例：![img](https://pic2.zhimg.com/v2-14f68845f363e0e6c7fd819eb60d5559_1440w.jpg)对于一个已经有锯齿的图像来说，使用低通卷积滤波得到的结果仍然是带有锯齿的，无法通过这种方式来获得一个高质量的结果。从信号的角度上来分析，以一维时间信号为例：当我们说采样的时候，实际上指的是对原始的信号 施加一个周期性的*脉冲响应*(impulse function) ∑+∞*k*=−∞*δ*(*t*−*k**T*) ，这样便能将原先的连续信号转化为离散信号。采样信号在时域上与原函数相乘，频域上就是两者的[傅立叶变换](https://www.zhihu.com/search?q=傅立叶变换&search_source=Entity&hybrid_search_source=Entity&hybrid_search_extra={"sourceType"%3A"answer"%2C"sourceId"%3A"136796584"})做卷积。而与周期性的脉冲响应函数做卷积，可以理解为将原函数不停的做搬移，数学上写作 \frac{1}{T}\sum_{k=-\infty}^{+\infty} F(\omega - k\omega_{s})\sum_{k=-\infty}^{+\infty} \delta(t - k\frac{1}{f_{s}}) * f(t)\Rightarrow\frac{1}{T}\sum_{k=-\infty}^{+\infty} \delta(\omega - k\omega_{s}) *F(\omega)\\
![img](https://pica.zhimg.com/v2-ae7aec54e56f421260560e056f2e9b3c_1440w.jpg)时域上的采样对应的频域上的卷积
所谓高质量的采样，就是要保证能从采样后的信号中恢复出原始的傅立叶谱F(\omega)，从图中可以发现傅里叶变换后的函数周期为 \omega_{s} ，因此原函数的最大频率 \omega_{M} 要小于一般的采样频率，即 \omega_{2}\geq2\omega_{M} 。恢复的方法则是对采样后的频谱做低通滤波，然后反变换：![img](https://pic2.zhimg.com/v2-de3d9ec199441008d67d084410006043_1440w.jpg)左图中离散信号能够重建出连续信号；右图由于原函数已经发生混叠，再采用低通滤波器也不能恢复出原始信号这也说明了为什么不能从一个已经有锯齿的图像中通过卷积模糊(低通滤波器)来减少锯齿，因为其信号已经发生混叠。同样不行的，是对ShadowMap做卷积。如果说在ShadowMap上应用一个 3\times3 的平滑均值卷积核，所得到的结果是一个深度的平均，其本身没有任何意义。想象在一个物体的边界，当前像素 p_{i}了前景物体片段的深度，而p_{i-1}记录了后景物体的深度，对它们两个做平均得到的是一个不属于场景中任何一个物体的深度，其本身并无实际意义，并且拿平均后的结果做深度测试，得到的结果也是非0即1，无法起到一个平滑的作用。V_{x}\ne \chi^{+} \left\{ [w\otimes D_{SM}](q) - D_{Scene} (x)\right\}\\所以正确的做法是将阴影测试的**结果**做卷积滤波。先前我们只是在ShadowMap上寻找一个对应的片段做阴影测试，PCF的方法就是在该片段的周围，取其他片段都进行阴影测试，将阴影测试的结果再做一次平均或者加权平均，从而得到一个在 [0,1] 之间的测试结果。可以等效的认为，周围的这一块区域都是对当前片段的深度采样，也就提高了采样率，从而提高了阴影的质量。V_{x} = w\otimes\chi^{+}[ D_{SM}(q) - D_{Scene} (x)]\\![img](https://pic3.zhimg.com/v2-80a9de9dc550fe78e337801c3ce1d3ba_1440w.jpg)取多大的范围取决于如何平衡阴影质量与渲染速度，一般来说取得范围越大，阴影越自然，但是对纹理的访问次数也就越多，造成性能上的下降。也有一些经验可以提供指导，比如[OpenGLTutorial](https://link.zhihu.com/?target=http%3A//www.opengl-tutorial.org/cn/intermediate-tutorials/tutorial-16-shadow-mapping/)中选择了一组PoissonDisk，可以在比较低的采样率下也能产生非常好的结果。

![img](https://pic2.zhimg.com/v2-b47bd75edbab20e57183ce002ce563a1_1440w.jpg)PCSS:PCSS的全称是Percentage Closer Soft Shadows，其灵感来源于PCF。从PCF的结果可以看出，我们在减少阴影锯齿的同时，产生了一种软阴影的效果。实际上在现实生活中，由于绝大部分的光源是面光源，我们见到的更多都是软阴影。![img](https://pic4.zhimg.com/v2-10499e6ef92c12ff736c06de69feb09b_1440w.jpg)Games202-Lecture3：软硬阴影的区别以及软阴影产生的原因很自然的就能想到，当我们在Shadow Map上选择查询范围的时候，范围越大，其结果过渡的越平滑，阴影也就越软。因此PCSS的关键在于如何选择合适的查询范围。![img](https://pica.zhimg.com/v2-d1c7c7b5e2773cbd0a7f1b6e04d60526_1440w.jpg)图源：GraphicDesign从日常生活中可以发现，当阴影的投射物(笔)到阴影的接受物(本子)距离越近，产生的阴影越硬(笔尖处)，距离越远则越软。因此查询范围和距离成正相关。![img](https://pic3.zhimg.com/v2-1c1ea0b9b0fd0c84c18173afc4663374_1440w.jpg)论文中给出了一种准确的计算方式。光源，投射物和接受物之间存在着一个相似三角形的关系。因此在渲染过程中我们要做的就是：Blocker Search: 想要知道相似三角形中的 d_{Blocker} - d_{Receive} 部分（也就是投射物到接受物的距离，这个线标的好奇怪），我们首先要计算出Blocker的深度。论文中给出的办法是，首先在Shadow Map上选择一个合适的范围，分别与要渲染的那个片段做阴影测试，这一步和先前PCF的方式一样。然后记录下来那些阴影测试没有通过的像素值，即深度值小于渲染片段深度的那些像素，将他们认为是遮挡物（如果像素的深度值小于目标片段就忽略），然后对这些像素求一个平均，认为是 d_{Blocker} 。而 d_{Receive} 则是当前片段本身的深度值。Penumbra estimation: 通过上图右侧的那个公式，我们计算出半影的范围，其实也就是决定了做PCF要取多大的范围进行阴影测试。Filtering: 采用和PCF中同样的方式计算阴影。PCSS本身的思想还是比较简洁的，可以看出上述的步骤中存在着一些问题：1.我们本身就是想要知道在第三步中要取多大的范围进行阴影测试，但是在第一步中要先取一个范围来计算Blocker的深度，变成了一个鸡生蛋蛋生鸡的套娃模式；2.原先在PCF的内容中有提到，在进行范围查询的过程中，需要多次对ShadowMap的纹理进行访问，其本身会给GPU带来比较大的负担，而在PCSS中有两次范围查询的步骤，会造成渲染速度的瓶颈。关于第一个问题，PCSS的作者在文章中给出了一个确定第一步查询范围的方法：![img](https://pic3.zhimg.com/v2-f68cd7cf6cbff49f8a5b4ff541d580a6_1440w.jpg)红色区域则是要在ShadowMap上查询的范围，通过相似三角形关系可以得出W_{kernel} =\frac{ (D_{frag} - nearPlane)}{(D_{frag} - LightPos.z)} * W_{Light}\\而另一个多次纹理访问的问题，在下面同VSSM的方法一起说。另外，PCSS本身解决的问题，使用点光源模拟面光源的效果，因为本身点光源只会产生硬阴影，而使用面光源来生成Shadow Map存在着诸多困难。因此这里光源的宽度其实取决于渲染时假想的面光源宽度。并且PCSS中还有一个问题，当计算出第一步的遮挡物平均深度后，在计算第三步的查询范围时，是利用上图中的公式，而这个公式本身就隐含着一个假设：光源、遮挡物与接受物是平行的，且都是平面的。SDF Soft Shadows:SDF全称Signed Distance Field，数学上来说，是定义在空间中的一个标量场，在 3D(2D) 空间中将位置映射到其到最近平面（边缘）的距离。距离场通常是有符号的，表示某个位置是否在物体内，曲面外的点为正值，曲面上的点为0，曲面内的点为负数，可以通过3D纹理来保存。![img](https://picx.zhimg.com/v2-ed9efa868d2bd41250ed19723aeddbeb_1440w.jpg)虚幻引擎文档-网格体距离场SDF可以用于抗锯齿，物体混合，碰撞测试、网格表示等。并且由于它提供了许多全局信息，我们可以通过查询SDF就能得到其附近的物体位置，对于RayMarching算法来说，几乎不需要额外成本就可以产生像软阴影和环境光遮蔽这样的阴影效果。
[Ray Marching](https://zhida.zhihu.com/search?content_id=194458612&content_type=Article&match_order=1&q=Ray+Marching&zhida_source=entity)是一种光线追踪算法中的一种。与常见的光线追踪算法里面直接的计算光线和图元的相交不同，它使用的是一种步进的方法：从物体表面一点出发，朝着空间中一个方向前进一段距离，判断是否击中其他物体。单纯的Ray Marching算法存在很大的局限性，考虑到渲染性能，通常步进的次数是有限的, 如果延伸的单步距离太大会导致穿透很薄的物体，而距离太小可能导致步进次数用完也没有找到下一个相交的物体。但是有了SDF就不一样，由于SDF中每一个格子记录的是当前位置 p 到空间中所有其他物体表面的最小距离 s ，也就是说从这一点出发进行Ray Marching，在 s 的范围内都是“安全”的，不会击中其他物体，这也就大大简化了对于步进距离的调整。使用SDF生成的阴影出于如下考虑：物体上的一点朝着光源方向进行Ray Marching，如果击中了其他物体，说明它被遮挡，应当在阴影中（本影部分）；而如果过程中没有击中其他物体，则可以根据这条光线在步进过程中距离其他物体的最小距离，判断该点是不是在半影中：![img](https://pic1.zhimg.com/v2-e86cb098470ed920bd7530851a582206_1440w.jpg)虚幻引擎文档-网格体距离场不过更好不是计算最小距离，而是计算夹角，因为阴影的软硬不止和光线和其他物体的最小距离有关，同时和阴影的接受物和投射物之间的距离有关：![img](https://pica.zhimg.com/v2-e3935cd2b1141c30acb4dd16ad145382_1440w.jpg)Games202-Lecture5`float softshadow( in vec3 ro, in vec3 rd, float mint, float maxt, float k ) {    float res = 1.0;    for( float t=mint; t<maxt; )    {        float h = map(ro + rd*t);        if( h<0.001 )            return 0.0;        res = min( res, k*h/t );        t += h;    }    return res; }`这里贴一下iq大神[教程](https://link.zhihu.com/?target=https%3A//iquilezles.org/www/articles/rmshadows/rmshadows.htm)里面的代码。这里面使用了 min\left\{ \frac{k \cdot SDF(p)}{p-o}, 1.0\right\} 来代替arcsin(\frac{SDF(p)}{p-o}) ，也是出于性能考虑，毕竟三角函数计算起来还是比较慢的。k 则是用来调节阴影软硬程度的系数：![img](https://pic3.zhimg.com/v2-4b7dbddc93be4f11dff72314e535c8e8_1440w.jpg)https://iquilezles.org/www/articles/rmshadows/rmshadows.htmGDC2018中Sebastian Aaltonen在黏土模拟游戏中对该算法进行了进一步的优化。由于Ray Marching是一个离散的过程，每一次步进的距离是当前SDF储存的值，有可能导致过程中错了沿射线产生最暗半影的点。了为了使软影算法稳定，我们应该在光线上每一处都计算到半影因子。但是毕竟marching是一个离散的过程，我们还是可能会错过一些颜色最深的半影计算，这有可能会造成一些瑕疵。而Sebastian 的技术不是通过光线步进后的位置进行计算，而且在每次迭代时估计从表面到行进光线的最近点。换句话说，通过使用当前采样点和前一个采样点，通过对光线进行三角测量来计算最近距离估计：![img](https://pica.zhimg.com/v2-be6546651187116ad2bfaa1d194c3a84_1440w.jpg)通过相似三角形关系，我们可以得到一个比原先更加精确的结果`float softshadow( in vec3 ro, in vec3 rd, float mint, float maxt, float k ) {    float res = 1.0;    float ph = 1e20;    for( float t=mint; t<maxt; )    {        float h = map(ro + rd*t);        if( h<0.001 )            return 0.0;        float y = h*h/(2.0*ph);        float d = sqrt(h*h-y*y);        res = min( res, k*d/max(0.0,t-y) );        ph = h;        t += h;    }    return res; }`上述的几个Shadow Map中的方法都是针对方向光源，其实对于点光源也类似，只不过将光源那个Pass的正交投影换成透视投影即可。并且由于点光源向空间中所有方向都发射光线，因此ShadowMap的结果可以通过Cubemap的形式来保存，在阴影测试阶段，需要先确定片段对应的阴影在哪一张Cubemap上，后续的操作同方向光源中的类似。实际渲染应用中的阴影远不止于上述这些方法，例如透视阴影贴图（PSM）及后续的优化**、**VSSM的进一步优化方法、PCSS中通过采样并添加时空滤波的方式生成阴影、Shadow Volume算法、多光源的阴影算法等等，有很多内容可以深究，以后有空也会介绍一下其他的阴影算法。参考资料：[Percentage-Closer Soft Shadows (nvidia.com)](https://link.zhihu.com/?target=https%3A//developer.download.nvidia.com/shaderlibrary/docs/shadow_PCSS.pdf)[VSSM.pdf (jankautz.com)](https://link.zhihu.com/?target=https%3A//jankautz.com/publications/VSSM_PG2010.pdf)[网格体距离场 | 虚幻引擎文档 (unrealengine.com)](https://link.zhihu.com/?target=https%3A//docs.unrealengine.com/4.27/zh-CN/BuildingWorlds/LightingAndShadows/MeshDistanceFields/)[Inigo Quilez :: fractals, computer graphics, mathematics, shaders, demoscene and more (iquilezles.org)](https://link.zhihu.com/?target=https%3A//iquilezles.org/www/articles/rmshadows/rmshadows.htm)[GAMES202: 高质量实时渲染 (ucsb.edu)](https://link.zhihu.com/?target=https%3A//sites.cs.ucsb.edu/~lingqi/teaching/games202.html)[阴影映射 - LearnOpenGL CN (learnopengl-cn.github.io)](https://link.zhihu.com/?target=https%3A//learnopengl-cn.github.io/05%20Advanced%20Lighting/03%20Shadows/01%20Shadow%20Mapping/)[Tutorial 16 : Shadow mapping (opengl-tutorial.org)](https://link.zhihu.com/?target=http%3A//www.opengl-tutorial.org/cn/intermediate-tutorials/tutorial-16-shadow-mapping/)[cascaded_shadow_maps (nvidia.com)](https://link.zhihu.com/?target=https%3A//developer.download.nvidia.com/SDK/10.5/opengl/src/cascaded_shadow_maps/doc/cascaded_shadow_maps.pdf)[LearnOpenGL - CSM](https://link.zhihu.com/?target=https%3A//learnopengl.com/Guest-Articles/2021/CSM)[shadow.dvi (acm.org)](https://link.zhihu.com/?target=https%3A//dl.acm.org/doi/pdf/10.1145/1128923.1128975)[信号与系统 (豆瓣) (douban.com)](https://link.zhihu.com/?target=https%3A//book.douban.com/subject/1062827/)

## SSAO

# SSAO

| 原文 | [SSAO](http://learnopengl.com/#!Advanced-Lighting/SSAO) |
| :--- | :------------------------------------------------------ |
| 作者 | JoeyDeVries                                             |
| 翻译 | Krasjet                                                 |
| 校对 | 未校对                                                  |

Note

本节暂未进行完全的重写，错误可能会很多。如果可能的话，请对照原文进行阅读。如果有报告本节的错误，将会延迟至重写之后进行处理。

我们已经在前面的基础教程中简单介绍到了这部分内容：环境光照(Ambient Lighting)。环境光照是我们加入场景总体光照中的一个固定光照常量，它被用来模拟光的**散射(Scattering)**。在现实中，光线会以任意方向散射，它的强度是会一直改变的，所以间接被照到的那部分场景也应该有变化的强度，而不是一成不变的环境光。其中一种间接光照的模拟叫做**环境光遮蔽(Ambient Occlusion)**，它的原理是通过将褶皱、孔洞和非常靠近的墙面变暗的方法近似模拟出间接光照。这些区域很大程度上是被周围的几何体遮蔽的，光线会很难流失，所以这些地方看起来会更暗一些。站起来看一看你房间的拐角或者是褶皱，是不是这些地方会看起来有一点暗？

下面这幅图展示了在使用和不使用SSAO时场景的不同。特别注意对比褶皱部分，你会发现(环境)光被遮蔽了许多：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_example.png)

尽管这不是一个非常明显的效果，启用SSAO的图像确实给我们更真实的感觉，这些小的遮蔽细节给整个场景带来了更强的深度感。

环境光遮蔽这一技术会带来很大的性能开销，因为它还需要考虑周围的几何体。我们可以对空间中每一点发射大量光线来确定其遮蔽量，但是这在实时运算中会很快变成大问题。在2007年，Crytek公司发布了一款叫做**屏幕空间环境光遮蔽(Screen-Space Ambient Occlusion, SSAO)**的技术，并用在了他们的看家作孤岛危机上。这一技术使用了屏幕空间场景的深度而不是真实的几何体数据来确定遮蔽量。这一做法相对于真正的环境光遮蔽不但速度快，而且还能获得很好的效果，使得它成为近似实时环境光遮蔽的标准。

SSAO背后的原理很简单：对于铺屏四边形(Screen-filled Quad)上的每一个片段，我们都会根据周边深度值计算一个**遮蔽因子(Occlusion Factor)**。这个遮蔽因子之后会被用来减少或者抵消片段的环境光照分量。遮蔽因子是通过采集片段周围球型核心(Kernel)的多个深度样本，并和当前片段深度值对比而得到的。高于片段深度值样本的个数就是我们想要的遮蔽因子。

![img](https://learnopengl-cn.github.io/img/05/09/ssao_crysis_circle.png)

上图中在几何体内灰色的深度样本都是高于片段深度值的，他们会增加遮蔽因子；几何体内样本个数越多，片段获得的环境光照也就越少。

很明显，渲染效果的质量和精度与我们采样的样本数量有直接关系。如果样本数量太低，渲染的精度会急剧减少，我们会得到一种叫做**波纹(Banding)**的效果；如果它太高了，反而会影响性能。我们可以通过引入随机性到采样核心(Sample Kernel)的采样中从而减少样本的数目。通过随机旋转采样核心，我们能在有限样本数量中得到高质量的结果。然而这仍然会有一定的麻烦，因为随机性引入了一个很明显的噪声图案，我们将需要通过模糊结果来修复这一问题。下面这幅图片([John Chapman](http://john-chapman-graphics.blogspot.com/)的佛像)展示了波纹效果还有随机性造成的效果：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_banding_noise.jpg)

你可以看到，尽管我们在低样本数的情况下得到了很明显的波纹效果，引入随机性之后这些波纹效果就完全消失了。

Crytek公司开发的SSAO技术会产生一种特殊的视觉风格。因为使用的采样核心是一个球体，它导致平整的墙面也会显得灰蒙蒙的，因为核心中一半的样本都会在墙这个几何体上。下面这幅图展示了孤岛危机的SSAO，它清晰地展示了这种灰蒙蒙的感觉：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_crysis.jpg)

由于这个原因，我们将不会使用球体的采样核心，而使用一个沿着表面法向量的半球体采样核心。

![img](https://learnopengl-cn.github.io/img/05/09/ssao_hemisphere.png)

通过在**法向半球体(Normal-oriented Hemisphere)**周围采样，我们将不会考虑到片段底部的几何体.它消除了环境光遮蔽灰蒙蒙的感觉，从而产生更真实的结果。这个SSAO教程将会基于法向半球法和John Chapman出色的[SSAO教程](http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html)。

## 样本缓冲

SSAO需要获取几何体的信息，因为我们需要一些方式来确定一个片段的遮蔽因子。对于每一个片段，我们将需要这些数据：

- 逐片段**位置**向量
- 逐片段的**法线**向量
- 逐片段的**反射颜色**
- **采样核心**
- 用来旋转采样核心的随机旋转矢量

通过使用一个逐片段观察空间位置，我们可以将一个采样半球核心对准片段的观察空间表面法线。对于每一个核心样本我们会采样线性深度纹理来比较结果。采样核心会根据旋转矢量稍微偏转一点；我们所获得的遮蔽因子将会之后用来限制最终的环境光照分量。

![img](https://learnopengl-cn.github.io/img/05/09/ssao_overview.png)

由于SSAO是一种屏幕空间技巧，我们对铺屏2D四边形上每一个片段计算这一效果；也就是说我们没有场景中几何体的信息。我们能做的只是渲染几何体数据到屏幕空间纹理中，我们之后再会将此数据发送到SSAO着色器中，之后我们就能访问到这些几何体数据了。如果你看了前面一篇教程，你会发现这和延迟渲染很相似。这也就是说SSAO和延迟渲染能完美地兼容，因为我们已经存位置和法线向量到G缓冲中了。

在这个教程中，我们将会在一个简化版本的延迟渲染器([延迟着色法](https://learnopengl-cn.github.io/05 Advanced Lighting/08 Deferred Shading/)教程中)的基础上实现SSAO，所以如果你不知道什么是延迟着色法，请先读完那篇教程。

由于我们已经有了逐片段位置和法线数据(G缓冲中)，我们只需要更新一下几何着色器，让它包含片段的线性深度就行了。回忆我们在深度测试那一节学过的知识，我们可以从`gl_FragCoord.z`中提取线性深度：

```c++
#version 330 core
layout (location = 0) out vec4 gPositionDepth;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

const float NEAR = 0.1; // 投影矩阵的近平面
const float FAR = 50.0f; // 投影矩阵的远平面
float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // 回到NDC
    return (2.0 * NEAR * FAR) / (FAR + NEAR - z * (FAR - NEAR));    
}

void main()
{    
    // 储存片段的位置矢量到第一个G缓冲纹理
    gPositionDepth.xyz = FragPos;
    // 储存线性深度到gPositionDepth的alpha分量
    gPositionDepth.a = LinearizeDepth(gl_FragCoord.z); 
    // 储存法线信息到G缓冲
    gNormal = normalize(Normal);
    // 和漫反射颜色
    gAlbedoSpec.rgb = vec3(0.95);
}
```

提取出来的线性深度是在观察空间中的，所以之后的运算也是在观察空间中。确保G缓冲中的位置和法线都在观察空间中(乘上观察矩阵也一样)。观察空间线性深度值之后会被保存在`gPositionDepth`颜色缓冲的alpha分量中，省得我们再声明一个新的颜色缓冲纹理。

通过一些小技巧来通过深度值重构实际位置值是可能的，Matt Pettineo在他的[博客](https://mynameismjp.wordpress.com/2010/09/05/position-from-depth-3/)里提到了这一技巧。这一技巧需要在着色器里进行一些计算，但是省了我们在G缓冲中存储位置数据，从而省了很多内存。为了示例的简单，我们将不会使用这些优化技巧，你可以自行探究。

`gPositionDepth`颜色缓冲纹理被设置成了下面这样：

```c++
glGenTextures(1, &gPositionDepth);
glBindTexture(GL_TEXTURE_2D, gPositionDepth);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
```

这给我们了一个线性深度纹理，我们可以用它来对每一个核心样本获取深度值。注意我们把线性深度值存储为了浮点数据；这样从0.1到50.0范围深度值都不会被限制在[0.0, 1.0]之间了。如果你不用浮点值存储这些深度数据，确保你首先将值除以`FAR`来标准化它们，再存储到`gPositionDepth`纹理中，并在以后的着色器中用相似的方法重建它们。同样需要注意的是`GL_CLAMP_TO_EDGE`的纹理封装方法。这保证了我们不会不小心采样到在屏幕空间中纹理默认坐标区域之外的深度值。

接下来我们需要真正的半球采样核心和一些方法来随机旋转它。

## 法向半球

我们需要沿着表面法线方向生成大量的样本。就像我们在这个教程的开始介绍的那样，我们想要生成形成半球形的样本。由于对每个表面法线方向生成采样核心非常困难，也不合实际，我们将在[切线空间](https://learnopengl-cn.github.io/05 Advanced Lighting/04 Normal Mapping/)(Tangent Space)内生成采样核心，法向量将指向正z方向。

![img](https://learnopengl-cn.github.io/img/05/09/ssao_hemisphere.png)

假设我们有一个单位半球，我们可以获得一个拥有最大64样本值的采样核心：

```c++
std::uniform_real_distribution<GLfloat> randomFloats(0.0, 1.0); // 随机浮点数，范围0.0 - 1.0
std::default_random_engine generator;
std::vector<glm::vec3> ssaoKernel;
for (GLuint i = 0; i < 64; ++i)
{
    glm::vec3 sample(
        randomFloats(generator) * 2.0 - 1.0, 
        randomFloats(generator) * 2.0 - 1.0, 
        randomFloats(generator)
    );
    sample = glm::normalize(sample);
    sample *= randomFloats(generator);
    GLfloat scale = GLfloat(i) / 64.0; 
    ssaoKernel.push_back(sample);  
}
```

我们在切线空间中以-1.0到1.0为范围变换x和y方向，并以0.0和1.0为范围变换样本的z方向(如果以-1.0到1.0为范围，取样核心就变成球型了)。由于采样核心将会沿着表面法线对齐，所得的样本矢量将会在半球里。

目前，所有的样本都是平均分布在采样核心里的，但是我们更愿意将更多的注意放在靠近真正片段的遮蔽上，也就是将核心样本靠近原点分布。我们可以用一个加速插值函数实现它：

```c++
   ...[接上函数]
   scale = lerp(0.1f, 1.0f, scale * scale);
   sample *= scale;
   ssaoKernel.push_back(sample);  
}
```

`lerp`被定义为：

```c++
GLfloat lerp(GLfloat a, GLfloat b, GLfloat f)
{
    return a + f * (b - a);
}
```

这就给了我们一个大部分样本靠近原点的核心分布。

![img](https://learnopengl-cn.github.io/img/05/09/ssao_kernel_weight.png)

每个核心样本将会被用来偏移观察空间片段位置从而采样周围的几何体。我们在教程开始的时候看到，如果没有变化采样核心，我们将需要大量的样本来获得真实的结果。通过引入一个随机的转动到采样核心中，我们可以很大程度上减少这一数量。

## 随机核心转动

通过引入一些随机性到采样核心上，我们可以大大减少获得不错结果所需的样本数量。我们可以对场景中每一个片段创建一个随机旋转向量，但这会很快将内存耗尽。所以，更好的方法是创建一个小的随机旋转向量纹理平铺在屏幕上。

我们创建一个4x4朝向切线空间平面法线的随机旋转向量数组：

```c++
std::vector<glm::vec3> ssaoNoise;
for (GLuint i = 0; i < 16; i++)
{
    glm::vec3 noise(
        randomFloats(generator) * 2.0 - 1.0, 
        randomFloats(generator) * 2.0 - 1.0, 
        0.0f); 
    ssaoNoise.push_back(noise);
}
```

由于采样核心是沿着正z方向在切线空间内旋转，我们设定z分量为0.0，从而围绕z轴旋转。

我们接下来创建一个包含随机旋转向量的4x4纹理；记得设定它的封装方法为`GL_REPEAT`，从而保证它合适地平铺在屏幕上。

```c++
GLuint noiseTexture; 
glGenTextures(1, &noiseTexture);
glBindTexture(GL_TEXTURE_2D, noiseTexture);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
```

现在我们有了所有的相关输入数据，接下来我们需要实现SSAO。

## SSAO着色器

SSAO着色器在2D的铺屏四边形上运行，它对于每一个生成的片段计算遮蔽值(为了在最终的光照着色器中使用)。由于我们需要存储SSAO阶段的结果，我们还需要在创建一个帧缓冲对象：

```c++
GLuint ssaoFBO;
glGenFramebuffers(1, &ssaoFBO);  
glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
GLuint ssaoColorBuffer;

glGenTextures(1, &ssaoColorBuffer);
glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
```

由于环境遮蔽的结果是一个灰度值，我们将只需要纹理的红色分量，所以我们将颜色缓冲的内部格式设置为`GL_RED`。

渲染SSAO完整的过程会像这样：

```c++
// 几何处理阶段: 渲染到G缓冲中
glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    [...]
glBindFramebuffer(GL_FRAMEBUFFER, 0);  

// 使用G缓冲渲染SSAO纹理
glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glClear(GL_COLOR_BUFFER_BIT);
    shaderSSAO.Use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPositionDepth);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    SendKernelSamplesToShader();
    glUniformMatrix4fv(projLocation, 1, GL_FALSE, glm::value_ptr(projection));
    RenderQuad();
glBindFramebuffer(GL_FRAMEBUFFER, 0);

// 光照处理阶段: 渲染场景光照
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
shaderLightingPass.Use();
[...]
glActiveTexture(GL_TEXTURE3);
glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
[...]
RenderQuad();
```

`shaderSSAO`这个着色器将对应G缓冲纹理(包括线性深度)，噪声纹理和法向半球核心样本作为输入参数：

```c++
#version 330 core
out float FragColor;
in vec2 TexCoords;

uniform sampler2D gPositionDepth;
uniform sampler2D gNormal;
uniform sampler2D texNoise;

uniform vec3 samples[64];
uniform mat4 projection;

// 屏幕的平铺噪声纹理会根据屏幕分辨率除以噪声大小的值来决定
const vec2 noiseScale = vec2(800.0/4.0, 600.0/4.0); // 屏幕 = 800x600

void main()
{
    [...]
}
```

注意我们这里有一个`noiseScale`的变量。我们想要将噪声纹理平铺(Tile)在屏幕上，但是由于`TexCoords`的取值在0.0和1.0之间，`texNoise`纹理将不会平铺。所以我们将通过屏幕分辨率除以噪声纹理大小的方式计算`TexCoords`的缩放大小，并在之后提取相关输入向量的时候使用。

```c++
vec3 fragPos = texture(gPositionDepth, TexCoords).xyz;
vec3 normal = texture(gNormal, TexCoords).rgb;
vec3 randomVec = texture(texNoise, TexCoords * noiseScale).xyz;
```

由于我们将`texNoise`的平铺参数设置为`GL_REPEAT`，随机的值将会在全屏不断重复。加上`fragPog`和`normal`向量，我们就有足够的数据来创建一个TBN矩阵，将向量从切线空间变换到观察空间。

```c++
vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
vec3 bitangent = cross(normal, tangent);
mat3 TBN = mat3(tangent, bitangent, normal);
```

通过使用一个叫做Gramm-Schmidt处理(Gramm-Schmidt Process)的过程，我们创建了一个正交基(Orthogonal Basis)，每一次它都会根据`randomVec`的值稍微倾斜。注意因为我们使用了一个随机向量来构造切线向量，我们没必要有一个恰好沿着几何体表面的TBN矩阵，也就是不需要逐顶点切线(和双切)向量。

接下来我们对每个核心样本进行迭代，将样本从切线空间变换到观察空间，将它们加到当前像素位置上，并将片段位置深度与储存在原始深度缓冲中的样本深度进行比较。我们来一步步讨论它：

```c++
float occlusion = 0.0;
for(int i = 0; i < kernelSize; ++i)
{
    // 获取样本位置
    vec3 sample = TBN * samples[i]; // 切线->观察空间
    sample = fragPos + sample * radius; 

    [...]
}
```

这里的`kernelSize`和`radius`变量都可以用来调整效果；在这里我们分别保持他们的默认值为`64`和`1.0`。对于每一次迭代我们首先变换各自样本到观察空间。之后我们会加观察空间核心偏移样本到观察空间片段位置上；最后再用`radius`乘上偏移样本来增加(或减少)SSAO的有效取样半径。

接下来我们变换`sample`到屏幕空间，从而我们可以就像正在直接渲染它的位置到屏幕上一样取样`sample`的(线性)深度值。由于这个向量目前在观察空间，我们将首先使用`projection`矩阵uniform变换它到裁剪空间。

```c++
vec4 offset = vec4(sample, 1.0);
offset = projection * offset; // 观察->裁剪空间
offset.xyz /= offset.w; // 透视划分
offset.xyz = offset.xyz * 0.5 + 0.5; // 变换到0.0 - 1.0的值域
```

在变量被变换到裁剪空间之后，我们用`xyz`分量除以`w`分量进行透视划分。结果所得的标准化设备坐标之后变换到**[0.0, 1.0]**范围以便我们使用它们去取样深度纹理：

```c++
float sampleDepth = -texture(gPositionDepth, offset.xy).w;
```

我们使用`offset`向量的`x`和`y`分量采样线性深度纹理从而获取样本位置从观察者视角的深度值(第一个不被遮蔽的可见片段)。我们接下来检查样本的当前深度值是否大于存储的深度值，如果是的，添加到最终的贡献因子上。

```c++
occlusion += (sampleDepth >= sample.z ? 1.0 : 0.0);
```

这并没有完全结束，因为仍然还有一个小问题需要考虑。当检测一个靠近表面边缘的片段时，它将会考虑测试表面之下的表面的深度值；这些值将会(不正确地)影响遮蔽因子。我们可以通过引入一个范围检测从而解决这个问题，正如下图所示([John Chapman](http://john-chapman-graphics.blogspot.com/)的佛像)：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_range_check.png)

我们引入一个范围测试从而保证我们只当被测深度值在取样半径内时影响遮蔽因子。将代码最后一行换成：

```c++
float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
occlusion += (sampleDepth >= sample.z ? 1.0 : 0.0) * rangeCheck;    
```

这里我们使用了GLSL的`smoothstep`函数，它非常光滑地在第一和第二个参数范围内插值了第三个参数。如果深度差因此最终取值在`radius`之间，它们的值将会光滑地根据下面这个曲线插值在0.0和1.0之间：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_smoothstep.png)

如果我们使用一个在深度值在`radius`之外就突然移除遮蔽贡献的硬界限范围检测(Hard Cut-off Range Check)，我们将会在范围检测应用的地方看见一个明显的(很难看的)边缘。

最后一步，我们需要将遮蔽贡献根据核心的大小标准化，并输出结果。注意我们用1.0减去了遮蔽因子，以便直接使用遮蔽因子去缩放环境光照分量。

```c++
}
occlusion = 1.0 - (occlusion / kernelSize);
FragColor = occlusion;  
```

下面这幅图展示了我们最喜欢的纳米装模型正在打盹的场景，环境遮蔽着色器产生了以下的纹理：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_without_blur.png)

可见，环境遮蔽产生了非常强烈的深度感。仅仅通过环境遮蔽纹理我们就已经能清晰地看见模型一定躺在地板上而不是浮在空中。

现在的效果仍然看起来不是很完美，由于重复的噪声纹理再图中清晰可见。为了创建一个光滑的环境遮蔽结果，我们需要模糊环境遮蔽纹理。

## 环境遮蔽模糊

在SSAO阶段和光照阶段之间，我们想要进行模糊SSAO纹理的处理，所以我们又创建了一个帧缓冲对象来储存模糊结果。

```c++
GLuint ssaoBlurFBO, ssaoColorBufferBlur;
glGenFramebuffers(1, &ssaoBlurFBO);
glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
glGenTextures(1, &ssaoColorBufferBlur);
glBindTexture(GL_TEXTURE_2D, ssaoColorBufferBlur);
glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBufferBlur, 0);
```

由于平铺的随机向量纹理保持了一致的随机性，我们可以使用这一性质来创建一个简单的模糊着色器：

```c++
#version 330 core
in vec2 TexCoords;
out float fragColor;

uniform sampler2D ssaoInput;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoInput, 0));
    float result = 0.0;
    for (int x = -2; x < 2; ++x) 
    {
        for (int y = -2; y < 2; ++y) 
        {
            vec2 offset = vec2(float(x), float(y)) * texelSize;
            result += texture(ssaoInput, TexCoords + offset).r;
        }
    }
    fragColor = result / (4.0 * 4.0);
}
```

这里我们遍历了周围在-2.0和2.0之间的SSAO纹理单元(Texel)，采样与噪声纹理维度相同数量的SSAO纹理。我们通过使用返回`vec2`纹理维度的`textureSize`，根据纹理单元的真实大小偏移了每一个纹理坐标。我们平均所得的结果，获得一个简单但是有效的模糊效果：

![img](https://learnopengl-cn.github.io/img/05/09/ssao.png)

这就完成了，一个包含逐片段环境遮蔽数据的纹理；在光照处理阶段中可以直接使用。

## 应用环境遮蔽

应用遮蔽因子到光照方程中极其简单：我们要做的只是将逐片段环境遮蔽因子乘到光照环境分量上。如果我们使用上个教程中的Blinn-Phong延迟光照着色器并做出一点修改，我们将会得到下面这个片段着色器：

```c++
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D gPositionDepth;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D ssao;

struct Light {
    vec3 Position;
    vec3 Color;

    float Linear;
    float Quadratic;
    float Radius;
};
uniform Light light;

void main()
{             
    // 从G缓冲中提取数据
    vec3 FragPos = texture(gPositionDepth, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedo, TexCoords).rgb;
    float AmbientOcclusion = texture(ssao, TexCoords).r;

    // Blinn-Phong (观察空间中)
    vec3 ambient = vec3(0.3 * AmbientOcclusion); // 这里我们加上遮蔽因子
    vec3 lighting  = ambient; 
    vec3 viewDir  = normalize(-FragPos); // Viewpos 为 (0.0.0)，在观察空间中
    // 漫反射
    vec3 lightDir = normalize(light.Position - FragPos);
    vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Diffuse * light.Color;
    // 镜面
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    float spec = pow(max(dot(Normal, halfwayDir), 0.0), 8.0);
    vec3 specular = light.Color * spec;
    // 衰减
    float dist = length(light.Position - FragPos);
    float attenuation = 1.0 / (1.0 + light.Linear * dist + light.Quadratic * dist * dist);
    diffuse  *= attenuation;
    specular *= attenuation;
    lighting += diffuse + specular;

    FragColor = vec4(lighting, 1.0);
}
```

(除了将其改到观察空间)对比于之前的光照实现，唯一的真正改动就是场景环境分量与`AmbientOcclusion`值的乘法。通过在场景中加入一个淡蓝色的点光源，我们将会得到下面这个结果：

![img](https://learnopengl-cn.github.io/img/05/09/ssao_final.png)

你可以在[这里](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao)找到完整的源代码，和以下着色器：

- 几何：[顶点](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao_geometry&type=vertex)，[片段](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao_geometry&type=fragment)
- SSAO：[顶点](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao&type=vertex)，[片段](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao&type=fragment)
- 模糊：[顶点](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao&type=vertex)，[片段](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao_blur&type=fragment)
- 光照：[顶点](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao&type=vertex)，[片段](http://learnopengl.com/code_viewer.php?code=advanced-lighting/ssao_lighting&type=fragment)

屏幕空间环境遮蔽是一个可高度自定义的效果，它的效果很大程度上依赖于我们根据场景类型调整它的参数。对所有类型的场景并不存在什么完美的参数组合方式。一些场景只在小半径情况下工作，又有些场景会需要更大的半径和更大的样本数量才能看起来更真实。当前这个演示用了64个样本，属于比较多的了，你可以调调更小的核心大小从而获得更好的结果。

一些你可以调整(比如说通过uniform)的参数：核心大小，半径和/或噪声核心的大小。你也可以提升最终的遮蔽值到一个用户定义的幂从而增加它的强度：

```c++
occlusion = 1.0 - (occlusion / kernelSize);       
FragColor = pow(occlusion, power);
```

多试试不同的场景和不同的参数，来欣赏SSAO的可定制性。

尽管SSAO是一个很微小的效果，可能甚至不是很容易注意到，它在很大程度上增加了合适光照场景的真实性，它也绝对是一个在你工具箱中必备的技术。

## 附加资源

- [SSAO教程](http://john-chapman-graphics.blogspot.nl/2013/01/ssao-tutorial.html)：John Chapman优秀的SSAO教程；本教程很大一部分代码和技巧都是基于他的文章
- [了解你的SSAO效果](https://mtnphil.wordpress.com/2013/06/26/know-your-ssao-artifacts/)：关于提高SSAO特定效果的一篇很棒的文章
- [深度值重构SSAO](http://ogldev.atspace.co.uk/www/tutorial46/tutorial46.html)：OGLDev的一篇在SSAO之上的拓展教程，它讨论了通过仅仅深度值重构位置矢量，节省了存储开销巨大的位置矢量到G缓冲的过程

## 最后一部分Displaced Mapping

# (十五) Textures-DisplacementMap



ue007/three.tsgithub.com/ue007/three.ts/tree/main/18-Texture-DisplacementMap](https://link.zhihu.com/?target=https%3A//github.com/ue007/three.ts/tree/main/18-Texture-DisplacementMap)2. DisplacementMapDisplacement Map（置换贴图，也叫移位贴图）可以改变模型对象的几何形状，因此在提供最真实的效果的同时也会大幅增加渲染性能的开销。![img](https://pic1.zhimg.com/v2-14f66effbb494081b2fac24b45a54e52_1440w.jpg)
置换贴图能实现很多仅仅通过Bump和Normal无法实现的效果（尤其是模型对象的轮廓表现）。![img](https://pic1.zhimg.com/v2-80ebffda1f1335d94e2a6e98d9f77652_1440w.jpg)置换贴图也常作为高度图来生成地形，并结合凹凸贴图实现丰富的地形效果。![img](https://picx.zhimg.com/v2-2eb9717e6d39f5ea29a80a8062a283f1_1440w.jpg)通过下图，可以看出Displacement Map和[Bump Map](https://zhida.zhihu.com/search?content_id=170228089&content_type=Article&match_order=1&q=Bump+Map&zhida_source=entity)的区别：![img](https://picx.zhimg.com/v2-9e708ad40c6c9847540bd76d1ed71549_1440w.jpg)上代码：`import * as THREE from 'three'; import { Geometry } from 'three/examples/jsm/deprecated/Geometry'; import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'; import Stats from 'three/examples/jsm/libs/stats.module'; import { GUI } from 'three/examples/jsm/libs/dat.gui.module'; const scene: THREE.Scene = new THREE.Scene(); //scene.background = new THREE.Color(0xff0000) const axesHelper = new THREE.AxesHelper(5); scene.add(axesHelper); const ambientLight = new THREE.AmbientLight(0xffffff, 1); scene.add(ambientLight); const light = new THREE.PointLight(0xffffff, 2); light.position.set(0, 5, 10); scene.add(light); const camera: THREE.PerspectiveCamera = new THREE.PerspectiveCamera(  75,  window.innerWidth / window.innerHeight,  0.1,  1000 ); const renderer: THREE.WebGLRenderer = new THREE.WebGLRenderer(); renderer.setSize(window.innerWidth, window.innerHeight); document.body.appendChild(renderer.domElement); const controls = new OrbitControls(camera, renderer.domElement); controls.screenSpacePanning = true; //so that panning up and down doesn't zoom in/out //controls.addEventListener('change', render) const planeGeometry: THREE.PlaneGeometry = new THREE.PlaneGeometry(  3.6,  1.8,  360,  180 ); const material: THREE.MeshPhongMaterial = new THREE.MeshPhongMaterial(); //const texture = new THREE.TextureLoader().load("img/grid.png") const texture = new THREE.TextureLoader().load(  'images/worldColour.2700x1350.jpg' ); material.map = texture; // const envTexture = new THREE.CubeTextureLoader().load(["img/px_eso0932a.jpg", "img/nx_eso0932a.jpg", "img/py_eso0932a.jpg", "img/ny_eso0932a.jpg", "img/pz_eso0932a.jpg", "img/nz_eso0932a.jpg"]) // envTexture.mapping = THREE.CubeReflectionMapping // material.envMap = envTexture //const specularTexture = new THREE.TextureLoader().load("img/earthSpecular.jpg") // material.specularMap = specularTexture const displacementMap = new THREE.TextureLoader().load(  'images/gebco_bathy.2700x1350_8bit.jpg' ); material.displacementMap = displacementMap; const plane: THREE.Mesh = new THREE.Mesh(planeGeometry, material); scene.add(plane); camera.position.z = 3; window.addEventListener('resize', onWindowResize, false); function onWindowResize() {  camera.aspect = window.innerWidth / window.innerHeight;  camera.updateProjectionMatrix();  renderer.setSize(window.innerWidth, window.innerHeight);  render(); } const stats = Stats(); document.body.appendChild(stats.dom); var options = {  side: {    FrontSide: THREE.FrontSide,    BackSide: THREE.BackSide,    DoubleSide: THREE.DoubleSide,  }, }; const gui = new GUI(); const materialFolder = gui.addFolder('THREE.Material'); materialFolder.add(material, 'transparent'); materialFolder.add(material, 'opacity', 0, 1, 0.01); materialFolder.add(material, 'depthTest'); materialFolder.add(material, 'depthWrite'); materialFolder  .add(material, 'alphaTest', 0, 1, 0.01)  .onChange(() => updateMaterial()); materialFolder.add(material, 'visible'); materialFolder  .add(material, 'side', options.side)  .onChange(() => updateMaterial()); //materialFolder.open() var data = {  color: material.color.getHex(),  emissive: material.emissive.getHex(),  specular: material.specular.getHex(), }; var meshPhongMaterialFolder = gui.addFolder('THREE.meshPhongMaterialFolder'); meshPhongMaterialFolder.addColor(data, 'color').onChange(() => {  material.color.setHex(Number(data.color.toString().replace('#', '0x'))); }); meshPhongMaterialFolder.addColor(data, 'emissive').onChange(() => {  material.emissive.setHex(Number(data.emissive.toString().replace('#', '0x'))); }); meshPhongMaterialFolder.addColor(data, 'specular').onChange(() => {  material.specular.setHex(Number(data.specular.toString().replace('#', '0x'))); }); meshPhongMaterialFolder.add(material, 'shininess', 0, 1024); meshPhongMaterialFolder.add(material, 'wireframe'); meshPhongMaterialFolder  .add(material, 'flatShading')  .onChange(() => updateMaterial()); meshPhongMaterialFolder.add(material, 'reflectivity', 0, 1); meshPhongMaterialFolder.add(material, 'refractionRatio', 0, 1); meshPhongMaterialFolder.add(material, 'displacementScale', -1, 1, 0.01); meshPhongMaterialFolder.add(material, 'displacementBias', -1, 1, 0.01); meshPhongMaterialFolder.open(); var planeData = {  width: 3.6,  height: 1.8,  widthSegments: 360,  heightSegments: 180, }; const planePropertiesFolder = gui.addFolder('PlaneGeometry'); //planePropertiesFolder.add(planeData, 'width', 1, 30).onChange(regeneratePlaneGeometry) //planePropertiesFolder.add(planeData, 'height', 1, 30).onChange(regeneratePlaneGeometry) planePropertiesFolder  .add(planeData, 'widthSegments', 1, 360)  .onChange(regeneratePlaneGeometry); planePropertiesFolder  .add(planeData, 'heightSegments', 1, 180)  .onChange(regeneratePlaneGeometry); planePropertiesFolder.open(); function regeneratePlaneGeometry() {  let newGeometry = new THREE.PlaneGeometry(    planeData.width,    planeData.height,    planeData.widthSegments,    planeData.heightSegments  );  plane.geometry.dispose();  plane.geometry = newGeometry; } function updateMaterial() {  material.side = Number(material.side);  material.needsUpdate = true; } var animate = function () {  requestAnimationFrame(animate);   render();   stats.update(); }; function render() {  renderer.render(scene, camera); } animate(); `执行脚本效果如图：![img](https://picx.zhimg.com/v2-fb363e783229648433eb67b5c90a3f79_1440w.jpg)3. Spector分析DisplacementMap![img](https://pic3.zhimg.com/v2-472e5a6249a625ce4e9bffd1da1839ae_1440w.jpg)Displacement Map改变了顶点信息，所以应该到vertext Shader里面找找代码：`#ifdef USE_DISPLACEMENTMAP    uniform sampler2D displacementMap;    uniform float displacementScale;    uniform float displacementBias; #endif  #ifdef USE_DISPLACEMENTMAP        transformed += normalize( objectNormal ) * ( texture2D( displacementMap, vUv ).x * displacementScale + displacementBias );    #endif vec4 mvPosition = vec4( transformed, 1.0 );`