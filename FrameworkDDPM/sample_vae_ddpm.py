
from vae import VAE
# 不再从 train_ddpm_latent 导入 LatentUNet，而是在这里重新定义
# 这样可以确保采样代码和训练代码的结构完全一致
from forward_noising import (
    get_index_from_list,
    sqrt_one_minus_alphas_cumprod,
    betas,
    posterior_variance,
    sqrt_recip_alphas,
)
import torch
import torch.nn as nn
import math
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from dataloader import show_tensor_image
import numpy as np
import os
import argparse


class SinusoidalPositionEmbeddings(nn.Module):
    def __init__(self, dim):
        super().__init__()
        self.dim = dim

    def forward(self, time):
        device = time.device
        half_dim = self.dim // 2
        embeddings = math.log(10000) / (half_dim - 1)
        embeddings = torch.exp(torch.arange(half_dim, device=device) * -embeddings)
        embeddings = time[:, None] * embeddings[None, :]
        embeddings = torch.cat((embeddings.sin(), embeddings.cos()), dim=-1)
        return embeddings


class LatentBlock(nn.Module):
    def __init__(self, in_ch, out_ch, time_emb_dim):
        super().__init__()
        self.time_mlp = nn.Linear(time_emb_dim, out_ch)
        
        # 卷积层
        self.conv1 = nn.Conv2d(in_ch, out_ch, 3, padding=1)
        self.conv2 = nn.Conv2d(out_ch, out_ch, 3, padding=1)
        
        # 动态计算 GroupNorm 的 num_groups
        num_groups = min(8, out_ch)
        while out_ch % num_groups != 0:
            num_groups -= 1
        # 如果 num_groups 为 0，使用 LayerNorm（动态维度）
        if num_groups == 0:
            self.norm1 = nn.GroupNorm(1, out_ch)
            self.norm2 = nn.GroupNorm(1, out_ch)
        else:
            self.norm1 = nn.GroupNorm(num_groups, out_ch)
            self.norm2 = nn.GroupNorm(num_groups, out_ch)
        
        # 残差连接：如果输入输出通道数不同，使用 1x1 卷积调整
        # 注意：旧版本的训练代码中，residual_conv 是单个 Conv2d（没有归一化和激活）
        if in_ch != out_ch:
            self.residual_conv = nn.Conv2d(in_ch, out_ch, 1)
        else:
            self.residual_conv = None
        
        # 使用 SiLU (Swish) 激活函数，而不是 ReLU
        self.silu = nn.SiLU()
    
    def forward(self, x, t):
        # 保存输入用于残差连接
        residual = x
        
        # First Conv + SiLU
        h = self.silu(self.norm1(self.conv1(x)))
        
        # Time embedding + SiLU
        time_emb = self.silu(self.time_mlp(t))
        # Extend last 2 dimensions
        time_emb = time_emb[(...,) + (None,) * 2]
        # Add time channel
        h = h + time_emb
        
        # Second Conv + SiLU
        h = self.silu(self.norm2(self.conv2(h)))
        
        # 残差连接（关键修复：防止梯度衰减）
        if self.residual_conv is not None:
            residual = self.residual_conv(residual)
        h = h + residual
        
        return h


class LatentUNet(nn.Module):
    def __init__(self, latent_channels=4, num_classes=2):
        super().__init__()
        time_emb_dim = 32
        
        # 时间嵌入
        self.time_mlp = nn.Sequential(
            SinusoidalPositionEmbeddings(time_emb_dim),
            nn.Linear(time_emb_dim, time_emb_dim),
            nn.SiLU()
        )
        
        # 类别嵌入
        self.class_emb = nn.Embedding(num_classes, time_emb_dim)
        
        # 第一层：增加通道数，保持空间尺寸
        self.block1 = LatentBlock(latent_channels, 64, time_emb_dim)
        
        # 第二层：进一步增加通道数，保持空间尺寸
        self.block2 = LatentBlock(64, 128, time_emb_dim)
        
        # 中间层：不改变空间尺寸，只增加深度
        self.mid1 = LatentBlock(128, 128, time_emb_dim)
        self.mid2 = LatentBlock(128, 128, time_emb_dim)
        
        # 第三层：减少通道数（注意：旧版本使用相加，所以输入通道数是 128）
        self.block3 = LatentBlock(128, 64, time_emb_dim)
        
        # 输出层：恢复到原始通道数（注意：旧版本使用相加，所以输入通道数是 64）
        self.block4 = LatentBlock(64, latent_channels, time_emb_dim)
    
    def forward(self, x, t, class_label=None):
        # 时间嵌入
        t_emb = self.time_mlp(t)
        
        # 类别嵌入处理
        use_class_condition = False
        if class_label is not None:
            if torch.is_tensor(class_label):
                if (class_label != -1).any():
                    use_class_condition = True
            elif class_label != -1:
                use_class_condition = True
        
        if use_class_condition:
            # 安全处理类别标签（避免索引越界）
            class_label_safe = class_label.clone() if torch.is_tensor(class_label) else class_label
            if torch.is_tensor(class_label_safe):
                class_label_safe[class_label_safe == -1] = 0
            elif class_label_safe == -1:
                class_label_safe = 0
            
            c_emb = self.class_emb(class_label_safe)
            
            # 创建类别掩码
            if torch.is_tensor(class_label):
                class_mask = (class_label != -1).float().view(-1, 1)
            else:
                class_mask = torch.tensor([1.0], device=t_emb.device).view(-1, 1)
            
            # 只对有类别条件的样本添加类别嵌入
            t_emb = t_emb + c_emb * class_mask
        
        # 编码器（保存特征用于跳跃连接）
        h1 = self.block1(x, t_emb)  # (batch, 64, 4, 4)
        h2 = self.block2(h1, t_emb)  # (batch, 128, 4, 4)
        
        # 中间层：不改变空间尺寸，只增加深度
        h = self.mid1(h2, t_emb)  # (batch, 128, 4, 4)
        h = self.mid2(h, t_emb)  # (batch, 128, 4, 4)
        
        # 解码器 + 跳跃连接（注意：旧版本使用相加，不是拼接）
        # 将编码器的特征加到解码器的输入上
        h = self.block3(h + h2, t_emb)  # (batch, 64, 4, 4)
        h = self.block4(h + h1, t_emb)  # (batch, 4, 4, 4)
        
        return h


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='VAE + Latent DDPM Sampling Script')
    
    # 模型相关参数
    parser.add_argument('--model_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                        help='模型保存目录路径')
    parser.add_argument('--vae_name', type=str, default='best_vae.pth',
                        help='VAE模型文件名')
    parser.add_argument('--ddpm_name', type=str, default='best_latent_ddpm.pth',
                        help='潜在空间DDPM模型文件名')
    
    # 生成参数
    parser.add_argument('--dataset', type=str, default='datasets-3',
                        help='数据集名称（用于计算潜空间缩放因子）')
    parser.add_argument('--latent_channels', type=int, default=4,
                        help='潜在空间通道数')
    parser.add_argument('--num_classes', type=int, default=2,
                        help='类别数量')
    parser.add_argument('--img_size', type=int, default=64,
                        help='输入图像大小')
    parser.add_argument('--T', type=int, default=300,
                        help='扩散时间步数')
    parser.add_argument('--target_class', type=int, default=None,
                        help='目标类别（0或1），用于文生图任务')
    parser.add_argument('--guidance_scale', type=float, default=1.0,
                        help='无分类器引导强度（CFG scale），越大越遵循类别条件，默认1.0表示不使用CFG')
    
    # 输出目录
    parser.add_argument('--output_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\output',
                        help='输出目录路径')
    
    return parser.parse_args()


@torch.no_grad()
def sample_timestep_latent(model, z, t, class_label=None, guidance_scale=1.0):
    """
    潜在空间单步去噪（支持无分类器引导 CFG）
    
    Args:
        model: 潜在空间UNet模型
        z: 当前潜在向量
        t: 时间步
        class_label: 类别标签
        guidance_scale: 引导强度（CFG scale），越大越遵循类别条件
    """
    # 无分类器引导（CFG）：结合条件预测和无条件预测
    if class_label is not None and guidance_scale > 1.0:
        # 条件预测（传入类别标签）
        predicted_noise_cond = model(z, t, class_label)
        # 无条件预测（不传入类别标签）
        predicted_noise_uncond = model(z, t, None)
        # CFG 公式：ε = ε_uncond + w * (ε_cond - ε_uncond)
        predicted_noise = predicted_noise_uncond + guidance_scale * (predicted_noise_cond - predicted_noise_uncond)
    else:
        # 不使用 CFG 或没有类别标签，直接预测
        predicted_noise = model(z, t, class_label)
    
    # 获取当前时间步的预计算参数
    betas_t = get_index_from_list(betas, t, z.shape)
    sqrt_one_minus_alphas_cumprod_t = get_index_from_list(sqrt_one_minus_alphas_cumprod, t, z.shape)
    sqrt_recip_alphas_t = get_index_from_list(sqrt_recip_alphas, t, z.shape)
    
    # 计算均值
    mean = sqrt_recip_alphas_t * (z - betas_t * predicted_noise / sqrt_one_minus_alphas_cumprod_t)
    
    # 计算方差
    posterior_variance_t = get_index_from_list(posterior_variance, t, z.shape)
    
    # 当t=0时，不需要添加噪声
    if t[0].item() == 0:
        return mean
    else:
        noise = torch.randn_like(z)
        return mean + torch.sqrt(posterior_variance_t) * noise


@torch.no_grad()
def sample_plot_image_latent(vae, model, device, img_size, T, target_class=None, guidance_scale=1.0, scale_factor=1.0):
    """
    使用VAE + 潜在空间DDPM生成图像（支持无分类器引导 CFG）
    
    Args:
        vae: VAE解码器
        model: 潜在空间DDPM模型
        device: 设备
        img_size: 图像大小
        T: 扩散步数
        target_class: 目标类别
        guidance_scale: 引导强度（CFG scale），越大越遵循类别条件
        scale_factor: 潜空间缩放因子（重要优化 1）
    """
    # 准备类别标签张量
    class_label = None
    if target_class is not None:
        class_label = torch.full((1,), target_class, device=device, dtype=torch.long)
    
    # 从标准正态分布初始化潜在特征图
    # 重要优化 1：在缩放后的潜在空间进行采样
    # 潜在特征图形状为 (batch, latent_channels, 4, 4)
    z = torch.randn((1, model.block1.conv1.in_channels, 4, 4), device=device)
    
    # 逐步去噪（在缩放后的潜在空间进行）
    for i in reversed(range(1, T)):
        t = torch.full((1,), i, device=device, dtype=torch.long)
        z = sample_timestep_latent(model, z, t, class_label, guidance_scale)
    
    # 最后一步
    t = torch.full((1,), 0, device=device, dtype=torch.long)
    z = sample_timestep_latent(model, z, t, class_label, guidance_scale)
    
    # 重要优化 1：将缩放后的潜变量还原到原始的潜空间
    # 因为 DDPM 训练时使用的是缩放后的 z，所以采样时需要还原
    z = z / scale_factor
    
    # 使用VAE解码器解码为图像
    x_recon = vae.decode(z, class_label)
    
    # 保存生成的图片
    if target_class is not None:
        filename = f'generated_vae_ddpm_class_{target_class}_cfg{guidance_scale}.png'
        title = f'Generated Image (Class {target_class}, CFG={guidance_scale})'
    else:
        filename = f'generated_vae_ddpm_cfg{guidance_scale}.png'
        title = f'Generated Image (CFG={guidance_scale})'
    
    plt.figure(figsize=(8, 8))
    show_tensor_image(x_recon)
    plt.axis('off')
    plt.title(title)
    output_path = os.path.join(args.output_dir, filename)
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()
    print(f"Generated image saved to: {output_path}")
    
    return x_recon


def test_image_generation():
    args = parse_args()
    
    # 创建output目录
    os.makedirs(args.output_dir, exist_ok=True)
    print(f"Output directory: {args.output_dir}")
    
    # 设置设备
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")
    
    # 加载VAE解码器
    vae = VAE(
        in_channels=3, 
        latent_channels=args.latent_channels, 
        num_classes=args.num_classes
    ).to(device)
    
    vae.load_state_dict(
        torch.load(
            os.path.join(args.model_dir, args.vae_name), 
            map_location=device, 
            weights_only=True
        )
    )
    vae.eval()
    print(f"VAE loaded from {os.path.join(args.model_dir, args.vae_name)}")
    
    # 加载潜在空间DDPM模型
    model = LatentUNet(latent_channels=args.latent_channels, num_classes=args.num_classes).to(device)
    model.load_state_dict(
        torch.load(
            os.path.join(args.model_dir, args.ddpm_name), 
            map_location=device, 
            weights_only=True
        )
    )
    model.eval()
    print(f"Latent DDPM loaded from {os.path.join(args.model_dir, args.ddpm_name)}")
    
    # 重要优化 1：计算潜空间的标准差，并进行缩放
    # 这是 DDPM 能够收敛的关键！
    print("Computing latent space statistics...")
    from dataloader import load_transformed_dataset
    dataloader = load_transformed_dataset(
        img_size=args.img_size, 
        batch_size=8, 
        dataset_name='datasets-3'  # 使用训练时的数据集
    )
    
    latent_stds = []
    with torch.no_grad():
        for batch, class_labels in dataloader:
            batch = batch.to(device)
            class_labels = class_labels.to(device)
            z = vae.encode_mu(batch, class_labels)
            latent_stds.append(z.std().item())
    
    avg_latent_std = sum(latent_stds) / len(latent_stds)
    print(f"Average latent std: {avg_latent_std:.4f}")
    
    # 计算缩放因子，使潜空间方差接近 1
    scale_factor = 1.0 / avg_latent_std
    print(f"Latent scaling factor: {scale_factor:.4f}")
    
    # 生成图片
    if args.target_class is not None:
        print(f"Generating image for class {args.target_class} with guidance_scale={args.guidance_scale}...")
        generated_img = sample_plot_image_latent(
            vae, model, device, args.img_size, args.T, target_class=args.target_class, guidance_scale=args.guidance_scale, scale_factor=scale_factor
        )
    else:
        print("Generating image without class conditioning...")
        generated_img = sample_plot_image_latent(
            vae, model, device, args.img_size, args.T, target_class=None, guidance_scale=args.guidance_scale, scale_factor=scale_factor
        )
    
    print("Image generation completed!")
    
    return generated_img


if __name__ == "__main__":
    args = parse_args()
    
    # 如果未指定target_class，则生成两个类别的图像
    if args.target_class is None:
        print("Generating images for both classes...")
        
        # 生成类别0的图像
        args.target_class = 0
        test_image_generation()
        
        # 生成类别1的图像
        args.target_class = 1
        test_image_generation()
    else:
        # 否则按指定生成
        test_image_generation()