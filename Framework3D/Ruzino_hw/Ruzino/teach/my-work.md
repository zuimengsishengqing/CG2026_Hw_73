![image-20260331224725524](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260331224725524.png)

求解稳定性优化：

**问题根源：**

在构建拉普拉斯矩阵时，代码存在一个严重的数学错误。根据教学文档中的公式：

1di∑j∈N(i)(vi−vj)=0,for all interior i.*d**i*1*j*∈*N*(*i*)∑(**v***i*−**v***j*)=**0**,for all interior *i*.

展开后应该是：

vi−1di∑j∈N(i)vj=0**v***i*−*d**i*1*j*∈*N*(*i*)∑**v***j*=**0**

这意味着矩阵方程应该是：

- 对角线元素 = 1
- 邻居元素 = -1/d_i（其中d_i是顶点的度数）

![image-20260331232923285](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260331232923285.png)

又优化了边缘点

![image-20260401111539608](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260401111539608.png)

但是似乎对于尖端点求解并不是很好

1. **ABF++角度优化**：

   - 计算每个三角形的初始角度
   - 设置角度约束（最小角度≥10°）
   - 使用牛顿迭代优化角度

2. **局部几何平滑**：

   - 检测极端点（曲率阈值或角度阈值）
   - 对极端点及其邻域进行拉普拉斯平滑
   - 重新计算法向量和参数化

   将替换右端项构建、求解与更新顶点坐标部分，增加稳健求解、正则化回退和位移限制。

我对AI引导错误，出现上图误以为是边界尖端点的偏差，但是仔细对比图发现后实际上是中间点求解也不对；

并且写代码前没有自己看论文导致选取错误求解器；



##### 遇到了各种问题：

1. **求解器选型错误**：用对称矩阵求解器（SimplicialLDLT）解非对称矩阵
2. **位移钳制破坏解**：限制了顶点位移，导致最终坐标不是方程的解
3. **矩阵与右端项权重分离**：两次独立遍历计算权重，导致不匹配
4. **边界点坐标引用隐患**：应该使用固定坐标而非当前坐标
5. **正则化时机错误**：应该在初始分解前就正则化
6. **邻居顺序错误**：Floater权重需要一致的逆时针顺序

优化成功：

<img src="C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260401153023417.png" alt="image-20260401153023417" style="zoom:50%;" />

2.正方形：

![image-20260401153058510](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260401153058510.png)

3.圆形

![image-20260401153121705](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260401153121705.png)



#### 不同权重公式

1uniform

weight = use_uniform ? (1.0 / degree) : weights[k];

2.cotangent
$$
 w_j = \cot \alpha_{ij} + \cot \beta_{ij}
$$
3.float
$$
w_k = \frac{\tan(\alpha_k/2) + \tan(\beta_k/2)}{|v_k - v_i|}
$$

##### 权重不同：



###### cotangent：

直接作用：

![image-20260407200450624](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260407200450624.png)

正方形：
![image-20260408180852633](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408180852633.png)

圆形：
![image-20260408180950830](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408180950830.png)



###### float：

直接：

![image-20260407203754926](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260407203754926.png)

圆形：

![image-20260407203840738](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260407203840738.png)



方形：

![image-20260407204009735](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260407204009735.png)

##### 最终效果

![image-20260407205118293](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260407205118293.png)



BunnyHead模型：
![image-20260407212019737](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260407212019737.png)

HeadCat：

![image-20260408173905748](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408173905748.png)

David：

uniform：
![image-20260408174249301](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408174249301.png)

float

![image-20260408174056920](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408174056920.png)

cotangent：

![image-20260408181147870](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408181147870.png)

###### Nefertiti_face

uniform

![image-20260408174642630](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408174642630.png)

cotangent

![image-20260408180625223](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408180625223.png)



float

![image-20260408174553851](C:\Users\宁尚哲\AppData\Roaming\Typora\typora-user-images\image-20260408174553851.png)



#### 崩溃：

1.有一次改代码生成后点击editor就会崩溃，怀疑是之前代码有bug，但是修复代码后重新生成依旧不行；

需要清理干净重新配置编译生成

2.float崩溃，但是只是输出了日志

1. 当`weight_type == 1`且`has_reference == true`时，会使用`reference_mesh`

2. 但是**`neighbor_old_indices`中的索引是基于`halfedge_mesh`的**

3. 如果`reference_mesh`和`halfedge_mesh`的拓扑结构不同，这些索引在`reference_mesh`中可能**无效**

4. 在`compute_floater_weights`中访问`mesh_ptr->vertex_handle(id)`时，如果id无效，OpenMesh可能直接崩溃而不是返回invalid handle

   空指针问题：从日志可以看出，程序在输出`[DEBUG] Weight mesh vertices:`后立即崩溃，说明`weight_mesh->n_vertices()`调用导致了崩溃。

   

   问题出在这行代码：

   ```
   auto* weight_mesh = (weight_type == 1 && has_reference) ? 
                       operand_to_openmesh(&reference_mesh).get() : 
                       halfedge_mesh.get();
   ```

   **关键问题**：

   - `operand_to_openmesh(&reference_mesh)`返回的是一个**临时对象**（可能是`std::unique_ptr`）
   - 调用`.get()`获取原始指针后，临时对象在语句结束时**立即被销毁**
   - `weight_mesh`变成了一个**悬空指针**（dangling pointer）
   - 虽然指针地址有效，但指向的对象已经被销毁
   - 访问`weight_mesh->n_vertices()`时导致崩溃

   **问题根源**：

   - 代码中使用了不存在的`OpenMeshType`类型
   - 使用了`std::unique_ptr`，但`operand_to_openmesh`返回的是`std::shared_ptr<PolyMesh>`

TODO：

高亏格曲面

拓展任务 为什么曲面的微分坐标定义为$v_i - \sum_{ij \in E_i} v_j$ 还有无其他好的权重，它们的效果如何？ 如果Mesh与盘不同胚，如何处理？ 有了这个参数化，你能进一步做些什么？

AI交互与优化