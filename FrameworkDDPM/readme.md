### 🚀 使用指南

注意，需要使用模型需要把模型放到**/model**目录下面（建议复制粘贴）

我代码是从model代码读取的，然后其他的model_。。。是不同类型的模型我存储，防止被覆盖。

数据集没有打包。

结果在**output文件夹**

##### 训练、基础图像生成与填充

```

# 步骤2：计算缩放因子并继续训练（需要归一化方差）
python train_vae.py --dataset datasets-3 --epochs 1000 --compute_scaling

# 步骤3：训练潜在空间DDPM
python train_ddpm_latent.py --dataset datasets-3 --epochs 5000

# 步骤4：生成图像与填充：
python sample_vae_ddpm.py --target_class 0
python sample_vae_ddpm.py --target_class 1
```



##### 使用VAE

###### 步骤1：训练 VAE（使用 L1 Loss + 梯度裁剪）

```
# 训练 VAE，使用 L1 Loss 保留更多细节
python train_vae.py --dataset datasets-3 --latent_channels 4 --num_classes 2 --batch_size 8 --epochs 2000 --lr 1e-4 --beta 0.1 --use_l1_loss --compute_scaling --scaling_samples 1000

# 或者使用 KL 退火策略
python train_vae.py --dataset datasets-3 --latent_channels 4 --num_classes 2 --batch_size 8 --epochs 100 --lr 1e-4 --beta 0.1 --use_l1_loss --kl_annealing --kl_anneal_epochs 50 --compute_scaling --scaling_samples 1000
```

###### 步骤2：训练 Latent DDPM（使用 mu + CFG + 梯度裁剪）

```
# 训练潜在空间 DDPM（自动使用 mu、CFG 和梯度裁剪）
python train_ddpm_latent.py --dataset datasets-3 --latent_channels 4 --num_classes 2 --batch_size 8 --epochs 5000 --lr 5e-4 --T 300
```

###### 步骤3：生成图像（使用 CFG）

```
# 不使用 CFG（guidance_scale=1.0）
python sample_vae_ddpm.py --latent_channels 4 --num_classes 2 --target_class 0 --guidance_scale 1.0

# 使用 CFG，中等强度（guidance_scale=3.0）
python sample_vae_ddpm.py --latent_channels 4 --num_classes 2 --target_class 0 --guidance_scale 3.0

# 使用 CFG，高强度（guidance_scale=7.5）
python sample_vae_ddpm.py --latent_channels 4 --num_classes 2 --target_class 0 --guidance_scale 7.5

# 生成类别1的图像
python sample_vae_ddpm.py --latent_channels 4 --num_classes 2 --target_class 1 --guidance_scale 5.0
```

#### Flow：

##### 训练命令：

```
# 从头开始训练
python train_flow_latent.py --dataset datasets-3 --batch_size 16 --epochs 2000 --save_interval 500

# 从检查点恢复训练
python train_flow_latent.py --resume latent_flow_epoch_1000.pth

# 指定学习率恢复
python train_flow_latent.py --resume latent_flow_epoch_1000.pth --lr 1e-4
```

##### 采样命令：

```
# 生成默认类别样本
python sample_flow_latent.py

# 生成指定类别和数量的样本
python sample_flow_latent.py --classes 0 1 --num_samples 4

# 从检查点加载参数
python sample_flow_latent.py --checkpoint_path model/latent_flow_epoch_1500.pth
```

