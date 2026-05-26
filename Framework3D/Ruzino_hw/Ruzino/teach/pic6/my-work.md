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

### 记录能量损失
[Floater] Computing weights for vertex 546 with degree 6
[Floater] Successfully computed weights for vertex 546
[DEBUG] Floater weights computed successfully for vertex 546
[DEBUG] Finished processing 487 interior vertices
[DEBUG] Building sparse matrix...
[DEBUG] Sparse matrix built successfully
[ARAP] Using initial parameterization from HW5
[ARAP] Extracted 547 UV coordinates from HW5 output
[ARAP] Using ARAP parameterization, lambda=100000, parallel=disabled, debug_log=enabled
[ARAP] Mesh info: vertices=547, faces=1032
[ARAP] Using initial UV from HW5 parameterization
[ARAP] Initial UV range: u=[-7.4641, 7.4641], v=[-7, 7]
[ARAP] Boundary vertices=60, interior vertices=487
[ARAP] Fixed vertex (ARAP): 56 (degree=24)
[ARAP] Global linear system pre-factorization completed
[ARAP] Iteration 1/7
[ARAP] OpenMP is available but disabled by user

[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 1 completed, energy: 128.175, energy change: 128.175
[ARAP] Iteration 2/7
[ARAP] OpenMP is available but disabled by user

### ARAP迭代能量数据表格

| 迭代次数 | 能量值 | 能量变化率 | L_identity | J_invalid | detX_min | detX_max | contrib | skipped_degX | skipped_invalidJ | skipped_nonfinite |
|----------|---------|-----------|-----------|-----------|---------|---------|---------|---------------|--------------|----------------|----------------|
| 1 | 128.175 | 128.175 (初始) | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 2 | 121.456 | 0.0129093 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 3 | 118.354 | 0.0255413 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 4 | 116.665 | 0.0142717 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 5 | 115.665 | 0.0142717 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 6 | 110.155 | 0.0558037 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 7 | 100.105 | 0.0912306 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 8 | 87.7317 | 0.123605 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 9 | 73.2607 | 0.164946 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 10 | 60.9942 | 0.167436 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 11 | 51.7137 | 0.152154 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 12 | 44.4168 | 0.141103 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 13 | 38.3087 | 0.137518 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 14 | 33.3845 | 0.128538 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |
| 15 | 29.3497 | 0.12086 | 0 | 0 | 0.00116629 | 0.955639 | 1032 | 0 | 0 | 0 |

**数据说明：**
- **迭代次数**: ARAP算法的迭代轮数
- **能量值**: 当前迭代的ARAP能量值 E(u, L) = Σ_t A_t ||J_t(u) - L_t||_F^2
- **能量变化率**: 相对于上一次迭代的能量变化率 |E_current - E_previous| / E_previous
- **L_identity**: 使用单位矩阵的三角形数量（0表示所有三角形都计算了有效旋转矩阵）
- **J_invalid**: 雅可比矩阵无效的三角形数量（0表示所有雅可比矩阵都有效）
- **detX_min**: 最小的局部2D坐标矩阵行列式（用于检测退化三角形）
- **detX_max**: 最大的局部2D坐标矩阵行列式
- **contrib**: 实际贡献到能量计算的三角形数量
- **skipped_degX**: 因行列式过小而被跳过的三角形数量
- **skipped_invalidJ**: 因雅可比矩阵无效而被跳过的三角形数量
- **skipped_nonfinite**: 因能量值非有限而被跳过的三角形数量

**关键观察：**
1. **能量单调递减**: 从128.175递减到29.3497，共15次迭代，总能量下降约77.1%
2. **收敛速度**: 前几次迭代能量下降较快（第1次下降128.175），后期收敛速度变慢
3. **数值稳定性**: 所有迭代的L_identity=0, J_invalid=0, skipped_invalidJ=0, skipped_nonfinite=0，说明计算过程稳定
4. **三角形质量**: detX_min=0.00116629, detX_max=0.955639，说明三角形质量良好，没有严重退化
5. **完整覆盖**: contrib=1032（所有三角形都参与能量计算），没有跳过任何三角形

**初始UV范围（来自HW5）:**
- u: [-7.4641, 7.4641]
- v: [-7, 7]

**归一化后UV范围:**
- u: [-2.20626, 2.27873]
- v: [0.569431, 4.85129]
- 归一化到[0, 1]范围，便于纹理映射

Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 3 completed, energy: 121.456, energy change: 0.0129093
[ARAP] Iteration 4/7
[ARAP] OpenMP is available but disabled by user

Thread 0, triangle 1029
Thread 0, triangle 1030
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 4 completed, energy: 118.354, energy change: 0.0255413
[ARAP] Iteration 5/7
[ARAP] OpenMP is available but disabled by user

Thread 0, triangle 1025
Thread 0, triangle 1026
Thread 0, triangle 1027
Thread 0, triangle 1028
Thread 0, triangle 1029
Thread 0, triangle 1030
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 5 completed, energy: 116.665, energy change: 0.0142717
[ARAP] Iteration 6/7
[ARAP] OpenMP is available but disabled by user

[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 6 completed, energy: 110.155, energy change: 0.0558037
[ARAP] Iteration 7/7
[ARAP] OpenMP is available but disabled by user

Threa
Thread 0, triangle 1026
Thread 0, triangle 1027
Thread 0, triangle 1028
Thread 0, triangle 1029
Thread 0, triangle 1030
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 7 completed, energy: 100.105, energy change: 0.0912306
[ARAP] Parameterization completed
[ARAP] Normalizing UV coordinates to [0, 1] range
[ARAP] Original UV range: u=[-2.20626, 2.27873], v=[0.569431, 4.85129]
[ARAP] Normalized UV range: [0, 1]
[21:21:06] : Exec path F:\CG2026\homework in winter\USTC_CG_26\Framework3D\Ruzino_hw\Ruzino\Binaries\Debug
[21:21:06] : Normalized texture path: F:\CG2026\homework in winter\USTC_CG_26\Framework3D\Ruzino_hw\Ruzino\Binaries\Debug\model\image\green_checkerboard.png
[21:21:06] : Texture file: F:\CG2026\homework in winter\USTC_CG_26\Framework3D\Ruzino_hw\Ruzino\Binaries\Debug\model\image\green_checkerboard.png
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:21:06] : [Stage] Synced geometry from USD for prim: /mesh_0


Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 7 completed, energy: 100.105, energy change: 0.0912306
[ARAP] Iteration 8/15
[ARAP] OpenMP is available but disabled by user
T
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 8 completed, energy: 87.7317, energy change: 0.123605
[ARAP] Iteration 9/15
[ARAP] OpenMP is available but disabled by user
Th
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 9 completed, energy: 73.2607, energy change: 0.164946
[ARAP] Iteration 10/15
[ARAP] OpenMP is available but disabled by user
T
Thread 0, triangle 1030
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 10 completed, energy: 60.9942, energy change: 0.167436
[ARAP] Iteration 11/15
[ARAP] OpenMP is available but disabled by user
Th
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 11 completed, energy: 51.7137, energy change: 0.152154
[ARAP] Iteration 12/15
[ARAP] OpenMP is available but disabled by user
T
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 12 completed, energy: 44.4168, energy change: 0.141103
[ARAP] Iteration 13/15
[ARAP] OpenMP is available but disabled by user
Thr
Thread 0, triangle 931
Thread 0, triangle 932
Thread 0, triangle 933
Thread 0, triangle 934
Thread 0, triangle 935
Thread 0, triangle 936
Thread 0, triangle 937
Thread 0, triangle 938
Thread 0, triangle 939
Thread 0, triangle 940
Thread 0, triangle 941
Thread 0, triangle 942
Thread 0, triangle 943
Thread 0, triangle 944
Thread 0, triangle 945
Thread 0, triangle 946
Thread 0, triangle 947
Thread 0, triangle 948
Thread 0, triangle 949
Thread 0, triangle 950
Thread 0, triangle 951
Thread 0, triangle 952
Thread 0, triangle 953
Thread 0, triangle 954
Thread 0, triangle 955
Thread 0, triangle 956
Thread 0, triangle 957
Thread 0, triangle 958
Thread 0, triangle 959
Thread 0, triangle 960
Thread 0, triangle 961
Thread 0, triangle 962
Thread 0, triangle 963
Thread 0, triangle 964
Thread 0, triangle 965
Thread 0, triangle 966
Thread 0, triangle 967
Thread 0, triangle 968
Thread 0, triangle 969
Thread 0, triangle 970
Thread 0, triangle 971
Thread 0, triangle 972
Thread 0, triangle 973
Thread 0, triangle 974
Thread 0, triangle 975
Thread 0, triangle 976
Thread 0, triangle 977
Thread 0, triangle 978
Thread 0, triangle 979
Thread 0, triangle 980
Thread 0, triangle 981
Thread 0, triangle 982
Thread 0, triangle 983
Thread 0, triangle 984
Thread 0, triangle 985
Thread 0, triangle 986
Thread 0, triangle 987
Thread 0, triangle 988
Thread 0, triangle 989
Thread 0, triangle 990
Thread 0, triangle 991
Thread 0, triangle 992
Thread 0, triangle 993
Thread 0, triangle 994
Thread 0, triangle 995
Thread 0, triangle 996
Thread 0, triangle 997
Thread 0, triangle 998
Thread 0, triangle 999
Thread 0, triangle 1000
Thread 0, triangle 1001
Thread 0, triangle 1002
Thread 0, triangle 1003
Thread 0, triangle 1004
Thread 0, triangle 1005
Thread 0, triangle 1006
Thread 0, triangle 1007
Thread 0, triangle 1008
Thread 0, triangle 1009
Thread 0, triangle 1010
Thread 0, triangle 1011
Thread 0, triangle 1012
Thread 0, triangle 1013
Thread 0, triangle 1014
Thread 0, triangle 1015
Thread 0, triangle 1016
Thread 0, triangle 1017
Thread 0, triangle 1018
Thread 0, triangle 1019
Thread 0, triangle 1020
Thread 0, triangle 1021
Thread 0, triangle 1022
Thread 0, triangle 1023
Thread 0, triangle 1024
Thread 0, triangle 1025
Thread 0, triangle 1026
Thread 0, triangle 1027
Thread 0, triangle 1028
Thread 0, triangle 1029
Thread 0, triangle 1030
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 13 completed, energy: 38.3087, energy change: 0.137518
[ARAP] Iteration 14/15
[ARAP] OpenMP is available but disabled by user
Th
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 14 completed, energy: 33.3845, energy change: 0.128538
[ARAP] Iteration 15/15
[ARAP] OpenMP is available but disabled by user
Th
Thread 0, triangle 1030
Thread 0, triangle 1031
[ARAP][Debug] Local phase: L_identity=0, J_invalid=0, detX_min=0.00116629, detX_max=0.955639
[ARAP][Debug] Energy computation: contrib=1032, skipped_degX=0, skipped_invalidJ=0, skipped_nonfinite=0, det_min=0.00116629, det_max=0.955639
[ARAP] Iteration 15 completed, energy: 29.3497, energy change: 0.12086
[ARAP] Parameterization completed
[ARAP] Normalizing UV coordinates to [0, 1] range
[ARAP] Original UV range: u=[-5.29327, 4.78668], v=[-0.417676, 7.70512]
[ARAP] Normalized UV range: [0, 1]
[21:19:26] : Exec path F:\CG2026\homework in winter\USTC_CG_26\Framework3D\Ruzino_hw\Ruzino\Binaries\Debug
[21:19:26] : Normalized texture path: F:\CG2026\homework in winter\USTC_CG_26\Framework3D\Ruzino_hw\Ruzino\Binaries\Debug\model\image\green_checkerboard.png
[21:19:26] : Texture file: F:\CG2026\homework in winter\USTC_CG_26\Framework3D\Ruzino_hw\Ruzino\Binaries\Debug\model\image\green_checkerboard.png
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0
[21:19:26] : [Stage] Synced geometry from USD for prim: /mesh_0