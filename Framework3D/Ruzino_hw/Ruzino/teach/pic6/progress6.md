#### ======各种模型验证：=========

##### Isis_dABF 

展开 ASAP

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419122415597.png" alt="image-20260419122415597" style="zoom:50%;" />



上贴图：

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419122540996.png" alt="image-20260419122540996" style="zoom:50%;" />

ARAP贴图

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419122740427.png" alt="image-20260419122740427" style="zoom:50%;" />

##### Cow展开图

ARAP展开

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419123028583.png" alt="image-20260419123028583" style="zoom:50%;" />

ARAP贴图

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419123146416.png" alt="image-20260419123146416" style="zoom:50%;" />

ASAP贴图

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419123237789.png" alt="image-20260419123237789" style="zoom:50%;" />



##### Beetle ABF

ASAP展开

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419123645722.png" alt="image-20260419123645722" style="zoom:50%;" />

ARAP贴图

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419123750363.png" alt="image-20260419123750363" style="zoom:50%;" />



ASAP贴图

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419123818232.png" alt="image-20260419123818232" style="zoom:50%;" />



=========================

ARAP展开验证：

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419121119912.png" alt="image-20260419121119912" style="zoom:50%;" />



ASAP展开图：

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260419121155391.png" alt="image-20260419121155391" style="zoom:67%;" />

绿色网格图：

![image-20260416141111076](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416141111076.png)

Interaction

0

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416141939008.png" alt="image-20260416141939008" style="zoom:50%;" />

2

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142005365.png" alt="image-20260416142005365" style="zoom:50%;" />

4

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142019072.png" alt="image-20260416142019072" style="zoom:50%;" />

8

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142032545.png" alt="image-20260416142032545" style="zoom:50%;" />

15

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142110835.png" alt="image-20260416142110835" style="zoom:50%;" />

25 

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142144353.png" alt="image-20260416142144353" style="zoom:50%;" />

40

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142210645.png" alt="image-20260416142210645" style="zoom:50%;" />



60

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416142232341.png" alt="image-20260416142232341" style="zoom:50%;" />

80

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416141111076.png" alt="image-20260416141111076" style="zoom:50%;" />

ARAP能量下降图：

![image-20260418215034495](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260418215034495.png)

### 能量震荡分析

##### 1. 论文严格结论（数学理想条件）

ARAP 2008 4.4 节明确：

> 固定 UV 求最优旋转（局部步）+ 固定旋转求最优 UV（全局步），**每一步能量严格非增**，算法必然收敛。

**这个结论成立的前提**：

- 无限精度实数计算（无浮点误差）
- 无硬约束、无惩罚项
- 线性系统求解**精确解**

##### 2. 你能量震荡的**4 个合法原因**（代码全中，正常）

代码和日志，逐条对应：

1. 浮点精度误差（核心原因）

   

   你用

   ```
   float
   ```

   存 UV、

   ```
   double
   ```

   算矩阵，

   舍入误差

   在接近收敛时（能量 < 15）被放大，导致微小能量上升；

2. 惩罚法硬约束引入数值误差

   

   你代码用 

   ```
   1e8
   ```

    惩罚权重固定 2 个顶点，

   破坏了矩阵严格正定性

   ，求解器会产生微小数值偏差；

3. 细长三角形放大误差

   

   日志

   ```
   det_min=0.001166
   ```

   ，存在

   极细长三角形

   ，雅可比矩阵接近奇异，SVD / 求逆误差翻倍；

4. 线性系统近似解

   

   ```
   SimplicialCholesky
   ```

   是

   数值近似解

   ，不是数学精确解，迭代后期误差会体现为能量波动。

##### 3. 你的日志数据铁证：完全健康

- 初始能量：139 → 最终能量：8.4，**整体下降 94%**，核心收敛完全达标；
- 震荡幅度：仅 ±2，**远小于能量量级**，属于**收敛区数值震荡**；
- 无报错：`J_invalid=0`、无退化三角、无 NaN，**算法全程稳定**。

### ARAP贴图小扭曲问题

**ARAP 是局部最优，非全局最优**

非凸优化只能收敛到局部极小，**高曲率区域必然有微小拉伸 / 扭曲**；

**细长三角形天然易畸变**

你的网格`det_min=0.001`，细长三角无论怎么优化，都会有纹理拉伸；

**固定顶点的影响**

你随机固定 2 个顶点，会导致局部区域应力集中，产生微小扭曲。



### ARAP与ASAP选取固定点分析（原本写的过约束）

##### 1. ASAP 与 ARAP 的固定点个数需求分析

虽然 ASAP 和 ARAP 在全局阶段都是解线性方程组，但它们优化目标的数学空间不同，因此需要的固定点数量**完全不同**。

#### ASAP (Least-Squares Conformal Maps)

- **需求个数**：**严格需要 2 个固定点**。

- 

  **背后原因**：ASAP 允许局部的三角形进行**相似变换**（包含平移、旋转、缩放）。如果你不施加任何约束，方程组的最优解（即能量最小值为 0 的平凡解）会将网格所有的顶点坍缩到二维平面上的同一个点（此时局部缩放比例 $scale = 0$）。

  

  

- 

  **固定点的作用**：为了防止这种坍缩，必须固定两个不同位置的顶点。这两个点一旦固定，就相当于为整个网格在 2D 平面上锁死了**平移（Translation）**、**旋转（Rotation）\**以及\**全局缩放比例（Scale）**。

  

  

#### ARAP (As-Rigid-As-Possible)

- **需求个数**：**仅需要 1 个固定点**（甚至不需要，如果使用伪逆的话）。

- 

  **背后原因**：ARAP 的局部变换受到严格限制，**仅允许刚性旋转（Rotation）**，缩放比例被强制锁定为 1 。在全局阶段求解方程组 $\mathbf{L}u = b$ 时，系数矩阵 $\mathbf{L}$ 是标准的拉普拉斯矩阵（Laplacian Matrix）。拉普拉斯矩阵的每一行元素之和为 0，它的零空间（Null Space）是一个常数向量 $\mathbf{1}$。这意味着方程组存在**平移歧义性**（把求出的所有 UV 坐标同时加上一个偏移量，方程依然成立）。

  

  

- **固定点的作用**：固定 1 个点仅仅是为了消除平移歧义性，把这块“已经展开好、形状确定”的布料“钉”在二维坐标系上的某个具体位置。

------

##### 2. 诊断你代码中的轻微扭曲

在你的 `hw6_arap.cpp` 代码中，针对全局系统的建立有这样一段：

C++

```
if (fixed_idx1 >= 0) {
    b_u_global(fixed_idx1) = PENALTY_WEIGHT * uv_coords[fixed_idx1].x;
    // ...
}
if (fixed_idx2 >= 0) {
    b_u_global(fixed_idx2) = PENALTY_WEIGHT * uv_coords[fixed_idx2].x;
    // ...
}
```

**问题就出在这里：你为 ARAP 固定了 2 个点。**

由于 ARAP 的局部求解已经计算出了每个三角形最优的旋转矩阵 $L_t$，这意味着整个网格展开后的“理想形状”在内部已经决定好了。两个点之间的理想几何距离在 3D 展开系下是客观存在的。

如果你强制固定了 `idx1` 和 `idx2` 两个点，并且约束它们必须位于你初始化的坐标（例如单纯的 XY 投影坐标），你实际上是**强制规定了这两个点在 2D 平面上的距离和相对角度**。

这就好比一块有弹性的布，它自然平展时的长度是 10cm，但你硬是用两根大头针把它钉在了相距 8cm 或者 12cm 的桌面上。为了满足你的这两个硬约束（因为你用了巨大的 `PENALTY_WEIGHT`），求解器只能把由此产生的形变误差强行分摊到整个网格的各个三角形中去。

这就是为什么你迭代 80 次，能量不再下降，但网格依然有轻微扭曲的根本原因——**这是一个被物理上锁死的残差。**

------

##### 3. 更优化的选取方案

针对你的代码，下面是优化建议：

#### 对于 ARAP：释放过约束

- **方案**：直接在代码中废弃掉 `fixed_idx2`。**只保留 `fixed_idx1`** 的惩罚法约束。
- **选取位置**：理论上选任何一个点都可以。但在数值稳定性上，建议选择**拓扑中心点**（度数较高且位于网格中心的内部顶点）。这样误差累积传播的路径最短，数值条件更好。不要选边界上的尖点。

#### 对于 ASAP（如果你后续需要实现）：

如果你打算实现一个 ASAP 节点，那么你必须固定两个点。

- 

  **选取方案**：正如原论文中所述：“在实践中，我们固定网格中距离最远的两个顶点（即网格的直径）” 。

  

  

- **原因**：固定距离最远的两个点，可以最大程度地减小数值求解时的杠杆效应（Leverage Effect），使得求解出的矩阵条件数更好，参数化的畸变分布更均匀。



#### 更新算法

// 选择一个固定点以避免平移歧义性（根据ARAP理论，只需要1个固定点）

  // ARAP的拉普拉斯矩阵零空间维度为1（常数向量），只存在平移歧义性

  // 使用2个固定点会导致过约束，产生无法消除的扭曲

  // 选择拓扑中心点（度数较高且位于网格中心的内部顶点）以获得最佳数值稳定性

// 如果没有内部顶点，选择边界顶点中度数最高的



#### 仅仅选取一个固定点分析

反而扭曲更大（相同迭代次数）

![image-20260416144028478](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416144028478.png)

但是固定1个点对于更高的迭代次数，内部会好，但是外部会有明显的扭曲：（收敛变慢了）
<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416144321837.png" alt="image-20260416144321837" style="zoom:50%;" />

迭代次数分析：
当次数过多的时候（300次等）

可能能量还能减少，但是可能反而是趋近于局部最优，扭曲更明显



#### 可能优化

##### 2. 诊断新问题：为什么 1 个点收敛变慢、短期效果反而不如 2 个点？

这是一个非常经典且有趣的几何处理工程问题。你观察到的现象非常敏锐，背后的原因可以用一句话概括：**你释放了“缩放（Scale）”的自由度，但也意外释放了“全局旋转（Rotation）”的自由度。**

我们来拆解一下这两种情况在求解器里的实际表现：

#### 场景 A：固定 2 个点（你之前的代码）

- **平移（Translation）**：被锁定。
- **旋转（Rotation）**：被锁定（两点连线的方向是死角）。
- **缩放（Scale）**：被锁定（两点之间的距离被强行规定）。
- **求解器表现**：因为网格完全不能在 2D 平面上旋转，局部旋转矩阵 $L_t$ 只能老老实实地去对齐这个固定的框架。求解器目标明确，**宏观收敛极快**。但因为“缩放”被锁死了（两点距离不对），所以多出来的形变无处安放，导致了局部的**轻微扭曲**。

#### 场景 B：只固定 1 个点（你现在的代码）

- **平移（Translation）**：被锁定。

- **缩放（Scale）**：自由释放！（解决了扭曲问题）。

- **旋转（Rotation）**：**自由释放！**（引发了新问题）。

- **求解器表现**：由于 ARAP 能量对“全局旋转”是免疫的（你把整个 2D 展开图旋转 30 度，能量完全不变）。这就导致在局部-全局交替迭代（Local-Global Alternating）的过程中，网格会产生**“旋转漂移（Rotational Drift）”**。

  你的求解器不仅在优化网格的内部形状，还在每一轮迭代中带着整个网格绕着 `fixed_idx1` 缓慢地“转圈”。这种寻找最优全局朝向的低频运动，极大地拖慢了高频细节（局部形状）的收敛速度。在有限的迭代次数下（比如 15-30 次），网格还在“转圈”，导致视觉效果看起来不如之前紧凑。

------

##### 3. 更优化的终极方案：1.5 个固定点（显式旋转对齐）

我们需要的是：**既不锁定缩放（避免扭曲），又要锁定旋转（加速收敛）。**

在工程实践中，最优雅且不需要修改核心 Sparse 求解器的做法是**“迭代后对齐（Post-Iteration Alignment）”**。

既然求解器会漂移，那我们就在每次全局求解后，手动把网格“拧”回到初始方向。因为 ARAP 能量具有旋转不变性，这个操作不会破坏能量下降，反而能彻底消除求解器的旋转歧义性。

**请将以下代码段加入到你的 `hw6_arap.cpp` 中：**

**第一步：在迭代循环开始前，选取一个参考点并记录初始朝向。**

（可以在 `// ARAP迭代` 之前，大约 line 364 的位置添加）

C++

```
    // --- 新增：用于消除全局旋转漂移的参考向量 ---
    int ref_idx = boundary_vertices[0]; // 随便选一个边界点作为朝向参考点
    glm::vec2 ref_vec_init = uv_coords[ref_idx] - uv_coords[fixed_idx1];
    float init_angle = std::atan2(ref_vec_init.y, ref_vec_init.x);
    // ------------------------------------------
    
    // 用于存储前一次迭代的能量值
    double previous_energy = 0.0;
```

**第二步：在全局阶段更新完 `uv_coords` 之后，强制对齐旋转角度。**

（在迭代循环内，更新完 `uv_coords[i] = glm::vec2(u_new(i), v_new(i));` 之后，大约 line 557 的位置添加）

C++

```
        for (int i = 0; i < n_vertices; ++i) {
            if (i == fixed_idx1) continue;
            uv_coords[i] = glm::vec2(u_new(i), v_new(i));
        }
        
        // --- 新增：显式旋转对齐 (Post-Iteration Alignment) ---
        // 计算当前迭代后的参考向量朝向
        glm::vec2 ref_vec_curr = uv_coords[ref_idx] - uv_coords[fixed_idx1];
        float curr_angle = std::atan2(ref_vec_curr.y, ref_vec_curr.x);
        
        // 计算需要补偿的旋转差值
        float delta_angle = init_angle - curr_angle; 
        float cos_a = std::cos(delta_angle);
        float sin_a = std::sin(delta_angle);
        
        // 将整个网格绕 fixed_idx1 旋转回去
        glm::vec2 center = uv_coords[fixed_idx1];
        for (int i = 0; i < n_vertices; ++i) {
            if (i == fixed_idx1) continue;
            glm::vec2 v = uv_coords[i] - center;
            float new_x = v.x * cos_a - v.y * sin_a;
            float new_y = v.x * sin_a + v.y * cos_a;
            uv_coords[i] = glm::vec2(new_x + center.x, new_y + center.y);
        }
        // ---------------------------------------------------
```

### ASAP

#### 1. **节点接口扩展**

- 添加了`UseASAP`布尔参数（默认false，保持ARAP行为）
- 一份代码同时支持两种算法，无需拆分文件

#### 2. **固定点选择策略**

- **ARAP模式**：选择1个拓扑中心点（度数最高的内部顶点）

- ASAP模式

  ：选择距离最远的2个顶点（网格直径）

  - 防止坍缩（ASAP允许缩放，scale=0是最优解）
  - 优化数值条件，减小杠杆效应

#### 3. **局部阶段L矩阵计算**

- **ARAP模式**：纯旋转矩阵 `L = U * V^T`

- ASAP模式

  ：相似变换矩阵

   

  ```
  L = s * U * V^T
  ```

  - 缩放系数：`s = (σ₁ + σ₂) / 2`（各向同性缩放）
  - 数值稳定性：`s = max(s, 1e-8)`（避免负缩放/零缩放）

#### 4. **完全复用的部分**

- 全局线性系统（余切拉普拉斯矩阵）
- 右端项构建逻辑
- 能量计算公式
- UV更新和归一化

### 🔍 **关键理论依据**

根据论文`arap.tex`：

#### **ASAP（As-Similar-As-Possible）**

- 允许**相似变换**（旋转 + 缩放）
- 最优变换：`L = (σ₁+σ₂)/2 * UV^T`
- 等价于**LSCM**（最小二乘共形映射）
- 需要**2个固定点**防止坍缩

#### **ARAP（As-Rigid-As-Possible）**

- 只允许**纯旋转**（刚性变换）
- 最优变换：`L = UV^T`（奇异值置1）
- 近等距参数化，保形性最优
- 只需**1个固定点**消除平移歧义

#### ARAP构建存在问题：

![image-20260416164235809](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416164235809.png)

更改架构

##### 优化后ARAP

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416171038765.png" alt="image-20260416171038765" style="zoom:80%;" />

##### ARAP相似UV上纹理贴图：

![image-20260416171224624](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260416171224624.png)

###### 能量一次方程计算：

```
[Floater] Computing weights for vertex 546 with degree 6
[Floater] Successfully computed weights for vertex 546
[DEBUG] Floater weights computed successfully for vertex 546
[DEBUG] Finished processing 487 interior vertices
[DEBUG] Building sparse matrix...
[DEBUG] Sparse matrix built successfully
[ARAP] Using initial parameterization from HW5
[ARAP] Extracted 547 UV coordinates from HW5 output
[ARAP] Using ASAP parameterization
[ARAP] Mesh info: vertices=547, faces=1032
[ARAP] Using initial UV from HW5 parameterization
[ARAP] Initial UV range: u=[-7.4641, 7.4641], v=[-7, 7]
[ARAP] Boundary vertices=60, interior vertices=487
[ARAP] Fixed vertices (ASAP): 126 and 280 (distance=11.8773)
[ARAP] Global linear system pre-factorization completed
[ARAP] ASAP is a linear problem, solving directly without iteration
[ASAP] Assembling direct linear system (u,a,b)
[ASAP] Direct solve completed, energy: 0.261629
[ARAP] Parameterization completed
[ARAP] Normalizing UV coordinates to [0, 1] range
[ARAP] Original UV range: u=[-9.47812, 5.23827], v=[-8.53187, 7.75717]
[ARAP] Normalized UV range: [0, 1]
```



### Hybrid实现

```
文件: hw6_arap.cpp — 修改点：新增输入 Lambda（double）；读取 Lambda；将纯 ASAP 直解改为仅在 Lambda==0 时启用；局部阶段用 λ 插值计算相似缩放 s（s = (avg_sigma + λ) / (1 + λ)），当 λ→0 为 ASAP，当 λ→+inf 近似为 ARAP（旋转）；其余全局求解逻辑保留（最小入侵改动）。

接口: 新增输入 Lambda（double，默认 0.0）。节点行为：

UseASAP = true && Lambda == 0 → 纯 ASAP（一次性直解，LSCM 等价）；
其它情况 → 迭代混合/ARAP 路径，局部变换受 Lambda 控制（数值稳定、退化保护保留）。
实现说明: 这是一个轻量近似混合实现（通过对奇异值做插值），与论文中用惩罚项严格解出 a,b 的解析三次方解法结果接近且实现改动小；若需要我可以继续按论文给出的解析形式替换本插值策略。
```

##### lambda无穷取值的分析

###### Lambda的数学含义

根据论文`arap.tex`第6节的混合能量函数：

$$ E = \frac{1}{2}\sum_{t=1}^T\left[ \sum_{i=0}^2 \cot\theta_t^i \fro{\nabla e_t^i} + \lambda(a_t^2+b_t^2-1)^2 \right] $$

其中 $a_t^2+b_t^2$ 是相似变换矩阵的缩放因子平方：

- **λ = 0**：惩罚项消失，允许任意缩放 → 纯ASAP（保角）
- **λ → +∞**：为了最小化能量，必须满足 $a_t^2+b_t^2=1$ → 纯ARAP（保面积）

###### 为什么1000不够大？

假设缩放偏离纯旋转10%（$a_t^2+b_t^2 = 1.1$）：

- **λ=1000**时：惩罚项 = $1000 \times (0.1)^2 = 1000 \times 0.01 = 10$
- **λ=100000**时：惩罚项 = $100000 \times (0.01) = 1000$

惩罚项相差100倍！所以1000确实不够强。

Hybrid

lambda = 100000 （ARAP）

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151336112.png" alt="image-20260417151336112" style="zoom:67%;" />

lambda = 	50000

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151536867.png" alt="image-20260417151536867" style="zoom:67%;" />

lambda = 10000

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151617672.png" alt="image-20260417151617672" style="zoom:67%;" />

lambda = 5000

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151705191.png" alt="image-20260417151705191" style="zoom:67%;" />

lambda = 1000

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151734344.png" alt="image-20260417151734344" style="zoom:67%;" />

lambda = 200

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151903429.png" alt="image-20260417151903429" style="zoom:67%;" />



lambda = 100

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151824801.png" alt="image-20260417151824801" style="zoom: 67%;" />

lambda = 50

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151955067.png" alt="image-20260417151955067" style="zoom:67%;" />

lambda = 25

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417152105419.png" alt="image-20260417152105419" style="zoom:67%;" />

lambda = 10

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417152135948.png" alt="image-20260417152135948" style="zoom:67%;" />

lambda = 0

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417151604406.png" alt="image-20260417151604406" style="zoom:67%;" />

## ARAP不同参数初始化验证

Interaction = 100

floater 

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417152911407.png" alt="image-20260417152911407" style="zoom:67%;" />

cotangent

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417152942014.png" alt="image-20260417152942014" style="zoom:67%;" />

uniform

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417153033520.png" alt="image-20260417153033520" style="zoom:67%;" />

## 并行加速实现

cmakelist配置（节点文件夹里面的cmakelist）

###### 关键区别部分：

```
# find OpenMP support
find_package(OpenMP REQUIRED)
message(STATUS "OpenMP found: ${OpenMP_FOUND}")
message(STATUS "OpenMP CXX flags: ${OpenMP_CXX_FLAGS}")
message(STATUS "OpenMP CXX include dirs: ${OpenMP_CXX_INCLUDE_DIR}")
message(STATUS "OpenMP libraries: ${OpenMP_CXX_LIBRARIES}")

# For MSVC, manually set OpenMP flag if find_package doesn't work correctly
if(MSVC)
    if("${OpenMP_CXX_FLAGS}" STREQUAL "-openmp" OR "${OpenMP_CXX_FLAGS}" STREQUAL "")
        message(STATUS "MSVC detected, manually setting OpenMP flag to /openmp")
        set(OpenMP_CXX_FLAGS "/openmp")
    endif()
endif()



add_nodes(
	TARGET_NAME geometry_nodes 
	SRC_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
	DEP_LIBS stage nodes_system usd geometry usdShade Eigen3::Eigen autodiff igl::core igl_restricted::triangle RZFemBem
	COMPILE_DEFS ${GEOMETRY_NODES_COMPILE_DEFS}
	COMPILE_OPTIONS ${OpenMP_CXX_FLAGS}
)

# add OpenMP support to geometry_nodes target
target_link_options(geometry_nodes INTERFACE $<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>)

target_link_libraries(node_points_to_mesh PRIVATE usd hioOpenVDB)
```

```
#include <omp.h>
```

在局部阶段，引入OpenMP多线程调度

```
// ==================== 局部阶段（OpenMP并行加速） ====================
std::vector<Eigen::Matrix2d> L_matrices(n_faces);
// 统计变量：线程安全需要原子累加
std::atomic<int> local_L_identity_count{0};
std::atomic<int> local_J_invalid_count{0};
std::atomic<double> local_min_abs_det{std::numeric_limits<double>::infinity()};
std::atomic<double> local_max_abs_det{0.0};


// 【核心并行语句】仅加这一行，自动多线程调度
        #pragma omp parallel for default(none) shared(n_faces, triangles, uv_coords, L_matrices, use_asap, lambda_param, DET_EPS) schedule(static)

```

#### 并行计算输出证据：

多线程输出：

```
[ARAP] Iteration 76 completed, energy: 9.46642, energy change: 0.163061
[ARAP] Iteration 77/77
[ARAP] OpenMP is enabled, max threads: 32
Thread 0, triangle 0
Thread 13, triangle 424
Thread 6, triangle 198
Thread 16, triangle 520
Thread 30, triangle 968
Thread 30, triangle 969
Thread 30, triangle 970
Thread 30, triangle 971
Thread 24, triangle 776
Thread 24, triangle 777
Thread 24, triangle 778
Thread 24, triangle 779
Thread 24, triangle 780
Thread 24, triangle 781
Thread 24, triangle 782
Thread 24, triangle 783
Thread 24, triangle 784
Thread 24, triangle 785
Thread 17, triangle 552
Thread 24, triangle 786
Thread 5, triangle 165
Thread 9, triangle 296
Thread 5, triangle 166
Thread 14, triangle 456
Thread 14, triangle 457
Thread 14, triangle 458
Thread 21, triangle 680
Thread 21, triangle 681
Thread 12, triangle 392
Thread 12, triangle 393
Thread 12, triangle 394
Thread 12, triangle 395
Thread 31, triangle 1000
Thread 12, triangle 396
Thread 12, triangle 397
Thread 12, triangle 398
Thread 12, triangle 399
Thread 12, triangle 400
Thread 12, triangle 401
Thread 12, triangle 402
Thread 29, triangle 936
Thread 29, triangle 937
Thread 29, triangle 938
Thread 29, triangle 939
Thread 29, triangle 940
Thread 29, triangle 941
Thread 30, triangle 972
Thread 30, triangle 973
Thread 30, triangle 974
Thread 30, triangle 975
Thread 30, triangle 976
Thread 30, triangle 977
Thread 30, triangle 978
Thread 30, triangle 979
Thread 30, triangle 980
Thread 20, triangle 648
Thread 20, triangle 649
Thread 7, triangle 231
Thread 20, triangle 650
Thread 17, triangle 553
Thread 3, triangle 99
Thread 24, triangle 787
Thread 24, triangle 788
Thread 24, triangle 789
Thread 24, triangle 790
Thread 24, triangle 791
Thread 24, triangle 792
Thread 24, triangle 793
Thread 24, triangle 794
Thread 24, triangle 795
Thread 24, triangle 796
Thread 10, triangle 328
Thread 10, triangle 329
Thread 10, triangle 330
Thread 31, triangle 1001
Thread 0, triangle 1
Thread 13, triangle 425
Thread 6, triangle 199
Thread 16, triangle 521
Thread 16, triangle 522
Thread 27, triangle 872
Thread 12, triangle 403
Thread 12, triangle 404
Thread 29, triangle 942
Thread 12, triangle 405
Thread 8, triangle 264
Thread 25, triangle 808
Thread 26, triangle 840
Thread 22, triangle 712
Thread 26, triangle 841
Thread 11, triangle 360
Thread 7, triangle 232
Thread 28, triangle 904
Thread 28, triangle 905
Thread 17, triangle 554
Thread 3, triangle 100
Thread 17, triangle 555
Thread 18, triangle 584
Thread 5, triangle 167
Thread 5, triangle 168
Thread 15, triangle 488
Thread 19, triangle 616
Thread 14, triangle 459
Thread 1, triangle 33
Thread 21, triangle 682
Thread 24, triangle 797
Thread 2, triangle 66
Thread 10, triangle 331
Thread 31, triangle 1002
Thread 10, triangle 332
Thread 13, triangle 426
Thread 6, triangle 200
Thread 16, triangle 523
Thread 27, triangle 873
Thread 27, triangle 874
Thread 27, triangle 875
Thread 4, triangle 132
Thread 4, triangle 133
Thread 12, triangle 406
Thread 12, triangle 407
Thread 12, triangle 408
Thread 25, triangle 809
Thread 25, triangle 810
Thread 25, triangle 811
Thread 25, triangle 812
Thread 25, triangle 813
Thread 26, triangle 842
Thread 26, triangle 843
Thread 26, triangle 844
Thread 7, triangle 233
Thread 7, triangle 234
Thread 7, triangle 235
Thread 7, triangle 236
Thread 28, triangle 906
Thread 28, triangle 907
Thread 3, triangle 101
Thread 28, triangle 908
Thread 28, triangle 909
Thread 18, triangle 585
Thread 18, triangle 586
Thread 15, triangle 489
Thread 15, triangle 490
Thread 15, triangle 491
Thread 15, triangle 492
Thread 14, triangle 460
Thread 14, triangle 461
Thread 21, triangle 683
Thread 21, triangle 684
Thread 2, triangle 67
Thread 2, triangle 68
Thread 0, triangle 2
Thread 31, triangle 1003
Thread 10, triangle 333
Thread 13, triangle 427
Thread 6, triangle 201
Thread 16, triangle 524
Thread 23, triangle 744
Thread 16, triangle 525
Thread 27, triangle 876
Thread 4, triangle 134
Thread 8, triangle 265
Thread 12, triangle 409
Thread 8, triangle 266
Thread 12, triangle 410
Thread 8, triangle 267
Thread 12, triangle 411
Thread 25, triangle 814
Thread 11, triangle 361
Thread 26, triangle 845
Thread 20, triangle 651
Thread 26, triangle 846
Thread 9, triangle 297
Thread 3, triangle 102
Thread 17, triangle 556
Thread 3, triangle 103
Thread 5, triangle 169
Thread 18, triangle 587
Thread 19, triangle 617
Thread 18, triangle 588
Thread 1, triangle 34
Thread 14, triangle 462
Thread 24, triangle 798
Thread 21, triangle 685
Thread 2, triangle 69
Thread 21, triangle 686
Thread 31, triangle 1004
Thread 10, triangle 334
Thread 13, triangle 428
Thread 6, triangle 202
Thread 29, triangle 943
Thread 23, triangle 745
Thread 29, triangle 944
Thread 29, triangle 945
Thread 27, triangle 877
Thread 27, triangle 878
Thread 27, triangle 879
Thread 4, triangle 135
Thread 22, triangle 713
Thread 30, triangle 981
Thread 22, triangle 714
Thread 30, triangle 982
Thread 12, triangle 412
Thread 25, triangle 815
Thread 11, triangle 362
Thread 25, triangle 816
Thread 20, triangle 652
Thread 26, triangle 847
Thread 20, triangle 653
Thread 20, triangle 654
Thread 28, triangle 910
Thread 3, triangle 104
Thread 3, triangle 105
Thread 5, triangle 170
Thread 19, triangle 618
Thread 15, triangle 493
Thread 18, triangle 589
Thread 1, triangle 35
Thread 14, triangle 463
Thread 14, triangle 464
Thread 24, triangle 799
Thread 0, triangle 3
Thread 2, triangle 70
Thread 2, triangle 71
Thread 31, triangle 1005
Thread 2, triangle 72
Thread 13, triangle 429
Thread 6, triangle 203
Thread 16, triangle 526
Thread 23, triangle 746
Thread 16, triangle 527
Thread 27, triangle 880
Thread 4, triangle 136
Thread 22, triangle 715
Thread 4, triangle 137
Thread 22, triangle 716
Thread 4, triangle 138
Thread 12, triangle 413
Thread 7, triangle 237
Thread 7, triangle 238
Thread 7, triangle 239
Thread 7, triangle 240
Thread 25, triangle 817
Thread 9, triangle 298
Thread 25, triangle 818
Thread 25, triangle 819
Thread 20, triangle 655
Thread 28, triangle 911
Thread 3, triangle 106
Thread 5, triangle 171
Thread 19, triangle 619
Thread 5, triangle 172
Thread 18, triangle 590
Thread 5, triangle 173
Thread 14, triangle 465
Thread 5, triangle 174
Thread 0, triangle 4
Thread 0, triangle 5
Thread 31, triangle 1006
Thread 10, triangle 335
Thread 2, triangle 73
Thread 13, triangle 430
Thread 6, triangle 204
Thread 23, triangle 747
Thread 6, triangle 205
Thread 16, triangle 528
Thread 16, triangle 529
Thread 8, triangle 268
Thread 22, triangle 717
Thread 22, triangle 718
Thread 4, triangle 139
Thread 12, triangle 414
Thread 11, triangle 363
Thread 7, triangle 241
Thread 9, triangle 299
Thread 7, triangle 242
Thread 17, triangle 557
Thread 25, triangle 820
Thread 17, triangle 558
Thread 28, triangle 912
Thread 3, triangle 107
Thread 15, triangle 494
Thread 19, triangle 620
Thread 1, triangle 36
Thread 18, triangle 591
Thread 24, triangle 800
Thread 18, triangle 592
Thread 24, triangle 801
Thread 21, triangle 687
Thread 0, triangle 6
Thread 21, triangle 688
Thread 0, triangle 7
Thread 10, triangle 336
Thread 2, triangle 74
Thread 13, triangle 431
Thread 23, triangle 748
Thread 29, triangle 946
Thread 23, triangle 749
Thread 23, triangle 750
Thread 16, triangle 530
Thread 8, triangle 269
Thread 30, triangle 983
Thread 8, triangle 270
Thread 30, triangle 984
Thread 12, triangle 415
Thread 12, triangle 416
Thread 26, triangle 848
Thread 9, triangle 300
Thread 26, triangle 849
Thread 25, triangle 821
Thread 26, triangle 850
Thread 17, triangle 559
Thread 26, triangle 851
Thread 17, triangle 560
Thread 28, triangle 913
Thread 3, triangle 108
Thread 28, triangle 914
Thread 19, triangle 621
Thread 28, triangle 915
Thread 14, triangle 466
Thread 5, triangle 175
Thread 18, triangle 593
Thread 5, triangle 176
Thread 31, triangle 1007
Thread 21, triangle 689
Thread 5, triangle 177
Thread 10, triangle 337
Thread 5, triangle 178
Thread 13, triangle 432
Thread 5, triangle 179
Thread 29, triangle 947
Thread 5, triangle 180
Thread 23, triangle 751
Thread 5, triangle 181
Thread 23, triangle 752
Thread 22, triangle 719
Thread 4, triangle 140
Thread 22, triangle 720
Thread 11, triangle 364
Thread 12, triangle 417
Thread 11, triangle 365
Thread 9, triangle 301
Thread 11, triangle 366
Thread 25, triangle 822
Thread 11, triangle 367
Thread 17, triangle 561
Thread 3, triangle 109
Thread 15, triangle 495
Thread 3, triangle 110
Thread 19, triangle 622
Thread 28, triangle 916
Thread 3, triangle 111
Thread 18, triangle 594
Thread 3, triangle 112
Thread 18, triangle 595
Thread 31, triangle 1008
Thread 18, triangle 596
Thread 31, triangle 1009
Thread 18, triangle 597
Thread 21, triangle 690
Thread 18, triangle 598
Thread 10, triangle 338
Thread 18, triangle 599
Thread 13, triangle 433
Thread 13, triangle 434
Thread 29, triangle 948
Thread 16, triangle 531
Thread 5, triangle 182
Thread 8, triangle 271
Thread 5, triangle 183
Thread 30, triangle 985
Thread 5, triangle 184
Thread 22, triangle 721
Thread 5, triangle 185
Thread 5, triangle 186
Thread 9, triangle 302
Thread 20, triangle 656
Thread 26, triangle 852
Thread 26, triangle 853
Thread 25, triangle 823
Thread 11, triangle 368
Thread 17, triangle 562
Thread 11, triangle 369
Thread 17, triangle 563
Thread 11, triangle 370
Thread 17, triangle 564
Thread 15, triangle 496
Thread 17, triangle 565
Thread 14, triangle 467
Thread 17, triangle 566
Thread 24, triangle 802
Thread 17, triangle 567
Thread 31, triangle 1010
Thread 0, triangle 8
Thread 31, triangle 1011
Thread 21, triangle 691
Thread 6, triangle 206
Thread 21, triangle 692
Thread 18, triangle 600
Thread 27, triangle 881
Thread 18, triangle 601
Thread 27, triangle 882
Thread 29, triangle 949
Thread 16, triangle 532
Thread 16, triangle 533
Thread 8, triangle 272
Thread 16, triangle 534
Thread 16, triangle 535
Thread 30, triangle 986
Thread 7, triangle 243
Thread 30, triangle 987
Thread 12, triangle 418
Thread 5, triangle 187
Thread 9, triangle 303
Thread 5, triangle 188
Thread 26, triangle 854
Thread 5, triangle 189
Thread 1, triangle 37
Thread 11, triangle 371
Thread 1, triangle 38
Thread 15, triangle 497
Thread 28, triangle 917
Thread 28, triangle 918
Thread 24, triangle 803
Thread 28, triangle 919
Thread 17, triangle 568
Thread 28, triangle 920
Thread 0, triangle 9
Thread 28, triangle 921
Thread 6, triangle 207
Thread 10, triangle 339
Thread 6, triangle 208
Thread 10, triangle 340
Thread 13, triangle 435
Thread 10, triangle 341
Thread 27, triangle 883
Thread 10, triangle 342
Thread 27, triangle 884
Thread 4, triangle 141
Thread 27, triangle 885
Thread 16, triangle 536
Thread 16, triangle 537
Thread 7, triangle 244
Thread 22, triangle 722
Thread 22, triangle 723
Thread 30, triangle 988
Thread 22, triangle 724
Thread 20, triangle 657
Thread 22, triangle 725
Thread 25, triangle 824
Thread 22, triangle 726
Thread 25, triangle 825
Thread 26, triangle 855
Thread 25, triangle 826
Thread 11, triangle 372
Thread 19, triangle 623
Thread 1, triangle 39
Thread 15, triangle 498
Thread 15, triangle 499
Thread 14, triangle 468
Thread 15, triangle 500
Thread 14, triangle 469
Thread 24, triangle 804
Thread 14, triangle 470
Thread 17, triangle 569
Thread 14, triangle 471
Thread 0, triangle 10
Thread 28, triangle 922
Thread 0, triangle 11
Thread 28, triangle 923
Thread 6, triangle 209
Thread 28, triangle 924
Thread 13, triangle 436
Thread 28, triangle 925
Thread 13, triangle 437
Thread 13, triangle 438
Thread 23, triangle 753
Thread 8, triangle 273
Thread 23, triangle 754
Thread 27, triangle 886
Thread 23, triangle 755
Thread 7, triangle 245
Thread 12, triangle 419
Thread 30, triangle 989
Thread 12, triangle 420
Thread 20, triangle 658
Thread 12, triangle 421
Thread 5, triangle 190
Thread 12, triangle 422
Thread 5, triangle 191
Thread 12, triangle 423
Thread 25, triangle 827
Thread 11, triangle 373
Thread 1, triangle 40
Thread 19, triangle 624
Thread 1, triangle 41
Thread 19, triangle 625
Thread 2, triangle 75
Thread 24, triangle 805
Thread 2, triangle 76
Thread 2, triangle 77
Thread 17, triangle 570
Thread 17, triangle 571
Thread 14, triangle 472
Thread 17, triangle 572
Thread 14, triangle 473
Thread 17, triangle 573
Thread 14, triangle 474
Thread 17, triangle 574
Thread 17, triangle 575
Thread 28, triangle 926
Thread 17, triangle 576
Thread 10, triangle 343
Thread 17, triangle 577
Thread 10, triangle 344
Thread 17, triangle 578
Thread 4, triangle 142
Thread 10, triangle 345
Thread 10, triangle 346
Thread 23, triangle 756
Thread 10, triangle 347
Thread 30, triangle 990
Thread 10, triangle 348
Thread 9, triangle 304
Thread 22, triangle 727
Thread 9, triangle 305
Thread 20, triangle 659
Thread 9, triangle 306
Thread 20, triangle 660
Thread 25, triangle 828
Thread 20, triangle 661
Thread 11, triangle 374
Thread 15, triangle 501
Thread 11, triangle 375
Thread 1, triangle 42
Thread 19, triangle 626
Thread 31, triangle 1012
Thread 19, triangle 627
Thread 2, triangle 78
Thread 19, triangle 628
Thread 2, triangle 79
Thread 19, triangle 629
Thread 0, triangle 12
Thread 18, triangle 602
Thread 14, triangle 475
Thread 6, triangle 210
Thread 18, triangle 603
Thread 29, triangle 950
Thread 13, triangle 439
Thread 29, triangle 951
Thread 17, triangle 579
Thread 16, triangle 538
Thread 17, triangle 580
Thread 27, triangle 887
Thread 23, triangle 757
Thread 27, triangle 888
Thread 30, triangle 991
Thread 10, triangle 349
Thread 22, triangle 728
Thread 10, triangle 350
Thread 9, triangle 307
Thread 5, triangle 192
Thread 9, triangle 308
Thread 25, triangle 829
Thread 9, triangle 309
Thread 25, triangle 830
Thread 3, triangle 113
Thread 25, triangle 831
Thread 11, triangle 376
Thread 1, triangle 43
Thread 24, triangle 806
Thread 31, triangle 1013
Thread 24, triangle 807
Thread 2, triangle 80
Thread 2, triangle 81
Thread 19, triangle 630
Thread 0, triangle 13
Thread 19, triangle 631
Thread 0, triangle 14
Thread 19, triangle 632
Thread 0, triangle 15
Thread 19, triangle 633
Thread 0, triangle 16
Thread 19, triangle 634
Thread 18, triangle 604
Thread 8, triangle 274
Thread 13, triangle 440
Thread 29, triangle 952
Thread 29, triangle 953
Thread 4, triangle 143
Thread 29, triangle 954
Thread 4, triangle 144
Thread 29, triangle 955
Thread 7, triangle 246
Thread 29, triangle 956
Thread 23, triangle 758
Thread 29, triangle 957
Thread 30, triangle 992
Thread 29, triangle 958
Thread 26, triangle 856
Thread 30, triangle 993
Thread 26, triangle 857
Thread 10, triangle 351
Thread 20, triangle 662
Thread 10, triangle 352
Thread 9, triangle 310
Thread 20, triangle 663
Thread 9, triangle 311
Thread 25, triangle 832
Thread 9, triangle 312
Thread 1, triangle 44
Thread 25, triangle 833
Thread 21, triangle 693
Thread 18, triangle 605
Thread 28, triangle 927
Thread 21, triangle 694
Thread 31, triangle 1014
Thread 1, triangle 45
Thread 6, triangle 211
Thread 9, triangle 313
Thread 6, triangle 212
Thread 2, triangle 82
Thread 6, triangle 213
Thread 19, triangle 635
Thread 2, triangle 83
Thread 13, triangle 441
Thread 8, triangle 275
Thread 16, triangle 539
Thread 20, triangle 664
Thread 17, triangle 581
Thread 4, triangle 145
Thread 17, triangle 582
Thread 29, triangle 959
Thread 17, triangle 583
Thread 5, triangle 193
Thread 23, triangle 759
Thread 26, triangle 858
Thread 23, triangle 760
Thread 26, triangle 859
Thread 23, triangle 761
Thread 26, triangle 860
Thread 22, triangle 729
Thread 26, triangle 861
Thread 22, triangle 730
Thread 26, triangle 862
Thread 25, triangle 834
Thread 22, triangle 731
Thread 25, triangle 835
Thread 22, triangle 732
Thread 25, triangle 836
Thread 22, triangle 733
Thread 25, triangle 837
Thread 22, triangle 734
Thread 28, triangle 928
Thread 21, triangle 695
Thread 31, triangle 1015
Thread 1, triangle 46
Thread 9, triangle 314
Thread 6, triangle 214
Thread 0, triangle 17
Thread 6, triangle 215
Thread 2, triangle 84
Thread 6, triangle 216
Thread 8, triangle 276
Thread 6, triangle 217
Thread 8, triangle 277
Thread 20, triangle 665
Thread 6, triangle 218
Thread 27, triangle 889
Thread 29, triangle 960
Thread 5, triangle 194
Thread 5, triangle 195
Thread 30, triangle 994
Thread 10, triangle 353
Thread 30, triangle 995
Thread 3, triangle 114
Thread 30, triangle 996
Thread 15, triangle 502
Thread 30, triangle 997
Thread 15, triangle 503
Thread 30, triangle 998
Thread 15, triangle 504
Thread 30, triangle 999
Thread 15, triangle 505
Thread 28, triangle 929
Thread 15, triangle 506
Thread 31, triangle 1016
Thread 1, triangle 47
Thread 9, triangle 315
Thread 19, triangle 636
Thread 9, triangle 316
Thread 13, triangle 442
Thread 2, triangle 85
Thread 2, triangle 86
Thread 8, triangle 278
Thread 2, triangle 87
Thread 8, triangle 279
Thread 20, triangle 666
Thread 6, triangle 219
Thread 27, triangle 890
Thread 29, triangle 961
Thread 27, triangle 891
Thread 29, triangle 962
Thread 10, triangle 354
Thread 29, triangle 963
Thread 3, triangle 115
Thread 11, triangle 377
Thread 7, triangle 247
Thread 18, triangle 606
Thread 18, triangle 607
Thread 25, triangle 838
Thread 22, triangle 735
Thread 21, triangle 696
Thread 22, triangle 736
Thread 28, triangle 930
Thread 15, triangle 507
Thread 31, triangle 1017
Thread 15, triangle 508
Thread 0, triangle 18
Thread 19, triangle 637
Thread 9, triangle 317
Thread 19, triangle 638
Thread 16, triangle 540
Thread 19, triangle 639
Thread 16, triangle 541
Thread 19, triangle 640
Thread 8, triangle 280
Thread 20, triangle 667
Thread 8, triangle 281
Thread 20, triangle 668
Thread 23, triangle 762
Thread 27, triangle 892
Thread 26, triangle 863
Thread 26, triangle 864
Thread 29, triangle 964
Thread 26, triangle 865
Thread 29, triangle 965
Thread 11, triangle 378
Thread 7, triangle 248
Thread 11, triangle 379
Thread 14, triangle 476
Thread 11, triangle 380
Thread 14, triangle 477
Thread 11, triangle 381
Thread 21, triangle 697
Thread 22, triangle 737
Thread 28, triangle 931
Thread 22, triangle 738
Thread 28, triangle 932
Thread 22, triangle 739
Thread 28, triangle 933
Thread 22, triangle 740
Thread 15, triangle 509
Thread 22, triangle 741
Thread 15, triangle 510
Thread 22, triangle 742
Thread 9, triangle 318
Thread 22, triangle 743
Thread 2, triangle 88
Thread 16, triangle 542
Thread 2, triangle 89
Thread 19, triangle 641
Thread 2, triangle 90
Thread 19, triangle 642
Thread 2, triangle 91
Thread 19, triangle 643
Thread 2, triangle 92
Thread 19, triangle 644
Thread 2, triangle 93
Thread 19, triangle 645
Thread 2, triangle 94
Thread 27, triangle 893
Thread 2, triangle 95
Thread 10, triangle 355
Thread 2, triangle 96
Thread 26, triangle 866
Thread 2, triangle 97
Thread 7, triangle 249
Thread 2, triangle 98
Thread 25, triangle 839
Thread 14, triangle 478
Thread 11, triangle 382
Thread 21, triangle 698
Thread 1, triangle 48
Thread 21, triangle 699
Thread 1, triangle 49
Thread 28, triangle 934
Thread 1, triangle 50
Thread 28, triangle 935
Thread 1, triangle 51
Thread 13, triangle 443
Thread 15, triangle 511
Thread 4, triangle 146
Thread 9, triangle 319
Thread 16, triangle 543
Thread 9, triangle 320
Thread 16, triangle 544
Thread 9, triangle 321
Thread 6, triangle 220
Thread 9, triangle 322
Thread 11, triangle 383
Thread 9, triangle 323
Thread 11, triangle 384
Thread 7, triangle 250
Thread 11, triangle 385
Thread 7, triangle 251
Thread 11, triangle 386
Thread 7, triangle 252
Thread 31, triangle 1018
Thread 7, triangle 253
Thread 13, triangle 444
Thread 7, triangle 254
Thread 13, triangle 445
Thread 7, triangle 255
Thread 19, triangle 646
Thread 5, triangle 196
Thread 19, triangle 647
Thread 16, triangle 545
Thread 20, triangle 669
Thread 8, triangle 282
Thread 20, triangle 670
Thread 0, triangle 19
Thread 21, triangle 700
Thread 4, triangle 147
Thread 6, triangle 221
Thread 14, triangle 479
Thread 6, triangle 222
Thread 14, triangle 480
Thread 14, triangle 481
Thread 26, triangle 867
Thread 26, triangle 868
Thread 29, triangle 966
Thread 11, triangle 387
Thread 31, triangle 1019
Thread 31, triangle 1020
Thread 3, triangle 116
Thread 13, triangle 446
Thread 3, triangle 117
Thread 13, triangle 447
Thread 3, triangle 118
Thread 16, triangle 546
Thread 3, triangle 119
Thread 16, triangle 547
Thread 3, triangle 120
Thread 20, triangle 671
Thread 0, triangle 20
Thread 21, triangle 701
Thread 0, triangle 21
Thread 21, triangle 702
Thread 0, triangle 22
Thread 21, triangle 703
Thread 0, triangle 23
Thread 14, triangle 482
Thread 0, triangle 24
Thread 1, triangle 52
Thread 0, triangle 25
Thread 29, triangle 967
Thread 11, triangle 388
Thread 18, triangle 608
Thread 11, triangle 389
Thread 18, triangle 609
Thread 23, triangle 763
Thread 18, triangle 610
Thread 13, triangle 448
Thread 18, triangle 611
Thread 13, triangle 449
Thread 16, triangle 548
Thread 3, triangle 121
Thread 20, triangle 672
Thread 4, triangle 148
Thread 15, triangle 512
Thread 9, triangle 324
Thread 15, triangle 513
Thread 9, triangle 325
Thread 21, triangle 704
Thread 14, triangle 483
Thread 26, triangle 869
Thread 14, triangle 484
Thread 26, triangle 870
Thread 31, triangle 1021
Thread 7, triangle 256
Thread 31, triangle 1022
Thread 7, triangle 257
Thread 31, triangle 1023
Thread 7, triangle 258
Thread 8, triangle 283
Thread 18, triangle 612
Thread 8, triangle 284
Thread 18, triangle 613
Thread 8, triangle 285
Thread 18, triangle 614
Thread 8, triangle 286
Thread 18, triangle 615
Thread 8, triangle 287
Thread 15, triangle 514
Thread 8, triangle 288
Thread 10, triangle 356
Thread 8, triangle 289
Thread 9, triangle 326
Thread 9, triangle 327
Thread 21, triangle 705
Thread 21, triangle 706
Thread 0, triangle 26
Thread 14, triangle 485
Thread 0, triangle 27
Thread 14, triangle 486
Thread 11, triangle 390
Thread 5, triangle 197
Thread 11, triangle 391
Thread 31, triangle 1024
Thread 27, triangle 894
Thread 31, triangle 1025
Thread 13, triangle 450
Thread 16, triangle 549
Thread 3, triangle 122
Thread 16, triangle 550
Thread 20, triangle 673
Thread 16, triangle 551
Thread 20, triangle 674
Thread 15, triangle 515
Thread 20, triangle 675
Thread 10, triangle 357
Thread 20, triangle 676
Thread 10, triangle 358
Thread 20, triangle 677
Thread 10, triangle 359
Thread 15, triangle 516
Thread 6, triangle 223
Thread 15, triangle 517
Thread 6, triangle 224
Thread 15, triangle 518
Thread 0, triangle 28
Thread 15, triangle 519
Thread 0, triangle 29
Thread 21, triangle 707
Thread 0, triangle 30
Thread 21, triangle 708
Thread 0, triangle 31
Thread 21, triangle 709
Thread 0, triangle 32
Thread 21, triangle 710
Thread 13, triangle 451
Thread 21, triangle 711
Thread 13, triangle 452
Thread 20, triangle 678
Thread 4, triangle 149
Thread 26, triangle 871
Thread 4, triangle 150
Thread 27, triangle 895
Thread 31, triangle 1026
Thread 27, triangle 896
Thread 31, triangle 1027
Thread 8, triangle 290
Thread 1, triangle 53
Thread 8, triangle 291
Thread 14, triangle 487
Thread 8, triangle 292
Thread 13, triangle 453
Thread 20, triangle 679
Thread 8, triangle 293
Thread 4, triangle 151
Thread 23, triangle 764
Thread 3, triangle 123
Thread 27, triangle 897
Thread 31, triangle 1028
Thread 27, triangle 898
Thread 31, triangle 1029
Thread 27, triangle 899
Thread 7, triangle 259
Thread 13, triangle 454
Thread 7, triangle 260
Thread 8, triangle 294
Thread 7, triangle 261
Thread 8, triangle 295
Thread 7, triangle 262
Thread 1, triangle 54
Thread 31, triangle 1030
Thread 27, triangle 900
Thread 6, triangle 225
Thread 13, triangle 455
Thread 6, triangle 226
Thread 23, triangle 765
Thread 3, triangle 124
Thread 23, triangle 766
Thread 3, triangle 125
Thread 23, triangle 767
Thread 31, triangle 1031
Thread 27, triangle 901
Thread 4, triangle 152
Thread 6, triangle 227
Thread 7, triangle 263
Thread 1, triangle 55
Thread 3, triangle 126
Thread 23, triangle 768
Thread 27, triangle 902
Thread 23, triangle 769
Thread 27, triangle 903
Thread 1, triangle 56
Thread 3, triangle 127
Thread 4, triangle 153
Thread 6, triangle 228
Thread 23, triangle 770
Thread 1, triangle 57
Thread 3, triangle 128
Thread 4, triangle 154
Thread 3, triangle 129
Thread 4, triangle 155
Thread 3, triangle 130
Thread 6, triangle 229
Thread 23, triangle 771
Thread 1, triangle 58
Thread 23, triangle 772
Thread 1, triangle 59
Thread 23, triangle 773
Thread 1, triangle 60
Thread 23, triangle 774
Thread 1, triangle 61
Thread 23, triangle 775
Thread 1, triangle 62
Thread 6, triangle 230
Thread 4, triangle 156
Thread 3, triangle 131
Thread 1, triangle 63
Thread 4, triangle 157
Thread 1, triangle 64
Thread 4, triangle 158
Thread 1, triangle 65
Thread 4, triangle 159
Thread 4, triangle 160
Thread 4, triangle 161
Thread 4, triangle 162
Thread 4, triangle 163
Thread 4, triangle 164
```

#### 日志优化与速率分析

![image-20260417172524224](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260417172524224.png)



节点加入了Enable Parallel代表是否打开并行计算优化，EnableDebugLog代表是否打开日志输出（建议关闭，不会输出循环大量内容）。

这样明显区分串行、并行，是否日志调试（因为日志调试输出会严重影响计算速度）

##### 速率分析

事实上是hw5的floater 的param的输出会用时久，后续对比是否开启并行优化发现，开启后会块一些，但是本身不开其实也耗时没那么久所以差别可能并不是很大