import torch
from forward_noising import (
    get_index_from_list,
    sqrt_one_minus_alphas_cumprod,
    betas,
    posterior_variance,
    sqrt_recip_alphas,
    forward_diffusion_sample,
)
import matplotlib
matplotlib.use('Agg')  # 使用非交互式后端，不显示窗口
import matplotlib.pyplot as plt
from dataloader import show_tensor_image, load_transformed_dataset
from unet import SimpleUnet
import numpy as np
import cv2 as cv
import os
import argparse

def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='DDPM Sampling Script')
    
    # 模型相关参数
    parser.add_argument('--model_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                        help='模型保存目录路径')
    parser.add_argument('--model_name', type=str, default='best_model.pth',
                        help='模型文件名')
    
    # 数据集相关参数
    parser.add_argument('--dataset', type=str, default='datasets-1',
                        choices=['datasets-1', 'datasets-2'],
                        help='选择数据集 (用于加载测试图像)')
    
    # 生成参数
    parser.add_argument('--img_size', type=int, default=64,
                        help='输入图像大小')
    parser.add_argument('--T', type=int, default=300,
                        help='扩散时间步数')
    parser.add_argument('--target_class', type=int, default=None,
                        help='目标类别（0或1），用于文生图任务')
    
    # 输出目录
    parser.add_argument('--output_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\output',
                        help='输出目录路径')
    
    return parser.parse_args()

# 解析命令行参数
args = parse_args()

# 模型存储目录配置
MODEL_DIR = args.model_dir
print(f"Model directory: {MODEL_DIR}")

# 创建output目录
output_dir = args.output_dir
os.makedirs(output_dir, exist_ok=True)
print(f"Output directory: {output_dir}")
print(f"Dataset: {args.dataset}")


# TODO: 你需要在这个函数中实现单步去噪过程
@torch.no_grad()
def sample_timestep(model, x, t, class_label=None):
    # 模型预测噪声：用训练好的UNet预测当前噪声，传入类别标签
    predicted_noise = model(x, t, class_label)
    
    # 获取当前时间步的预计算参数
    betas_t = get_index_from_list(betas, t, x.shape)
    sqrt_one_minus_alphas_cumprod_t = get_index_from_list(sqrt_one_minus_alphas_cumprod, t, x.shape)
    sqrt_recip_alphas_t = get_index_from_list(sqrt_recip_alphas, t, x.shape)
    
    # 计算均值：μ_θ(x_t, t) = 1/√α_t * (x_t - β_t/√(1-ᾱ_t) * ε_θ(x_t, t))
    mean = sqrt_recip_alphas_t * (x - betas_t * predicted_noise / sqrt_one_minus_alphas_cumprod_t)
    
    # 计算方差：σ_t^2 = β̃_t
    posterior_variance_t = get_index_from_list(posterior_variance, t, x.shape)
    
    # 当t=0时，不需要添加噪声（因为已经到达x_0）
    if t[0].item() == 0:
        return torch.clamp(mean, -1.0, 1.0)
    else:
        # 添加高斯噪声：z ~ N(0, I)
        noise = torch.randn_like(x)
        result = mean + torch.sqrt(posterior_variance_t) * noise
        return result

# TODO: 你需要在这个函数中完成对纯高斯噪声的去噪，并输出对应的去噪图片
# 你需要调用上面的sample_timestep函数，以实现单步去噪
@torch.no_grad()
def sample_plot_image(model, device, img_size, T, target_class=None):
    # 从纯高斯噪声开始：x_T ~ N(0, I)
    img = torch.randn((1, 3, img_size, img_size), device=device)
    
    # 准备类别标签张量（如果指定了目标类别）
    class_label = None
    if target_class is not None:
        class_label = torch.full((1,), target_class, device=device, dtype=torch.long)
    
    # 逐步去噪：从t=T-1到t=1
    # 注意：DDPM 的时间步范围是 [0, T-1]，所以最大时间步是 T-1
    for i in reversed(range(1, T)):
        t = torch.full((1,), i, device=device, dtype=torch.long)
        img = sample_timestep(model, img, t, class_label)
    
    # 最后一步：从t=0（直接使用模型预测，不添加噪声）
    t = torch.full((1,), 0, device=device, dtype=torch.long)
    img = sample_timestep(model, img, t, class_label)
    
    # 生成文件名（根据是否指定类别）
    if target_class is not None:
        filename = f'generated_image_class_{target_class}.png'
        title = f'Generated Image (Class {target_class})'
    else:
        filename = 'generated_image.png'
        title = 'Generated Image'
    
    # 保存生成的图片到output目录
    plt.figure(figsize=(8, 8))
    show_tensor_image(img)
    plt.axis('off')
    plt.title(title)
    output_path = os.path.join(output_dir, filename)
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()  # 关闭图形，不显示
    print(f"Generated image saved to: {output_path}")
    
    return img

# TODO: 你需要在这个函数中完成模型以及其他相关资源的加载，并调用sample_plot_image进行去噪，以生成图片
def test_image_generation():
    # 设置设备
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")
    
    # 加载训练好的模型（使用最佳模型）
    model = SimpleUnet().to(device)
    model.load_state_dict(torch.load(os.path.join(MODEL_DIR, args.model_name), map_location=device, weights_only=True))
    model.eval()
    
    # 设置参数
    img_size = args.img_size
    T = args.T
    target_class = args.target_class
    
    # 如果指定了目标类别，打印提示信息
    if target_class is not None:
        print(f"Generating image for class {target_class}...")
    else:
        print("Generating image without class conditioning...")
    
    # 生成图片
    print("Starting image generation...")
    generated_img = sample_plot_image(model, device, img_size, T, target_class=target_class)
    print("Image generation completed!")
    
    return generated_img

# TODO：你需要在这个函数中实现图像的补充
# Follows: RePaint: Inpainting using Denoising Diffusion Probabilistic Models
@torch.no_grad()
def inpaint(model, device, original_img, mask, t_max=100, jump_length=10, jump_n_sample=10, class_label=None):
    """
    RePaint图像补全方法 - 按照论文算法 1 实现
    
    Args:
        model: 训练好的UNet模型
        device: 设备（cuda或cpu）
        original_img: 原始图像 x_0（用于约束已知区域）
        mask: 掩码（1表示需要补全的未知区域，0表示需要保留的已知区域）
        t_max: 最大时间步（从 T 开始）
        jump_length: 跳跃长度，每次重采样后前向扩散的时间步数
        jump_n_sample: 每个时间步的重采样次数 U
        class_label: 类别标签（用于条件生成）
    """
    # 确保mask的维度与img一致
    if mask.shape[1] != original_img.shape[1]:
        mask = mask.repeat(1, original_img.shape[1], 1, 1)
    
    # 步骤 1：从纯高斯噪声初始化 x_T ~ N(0, I)
    img = torch.randn_like(original_img)
    
    # 步骤 2：从 t=T-1 逐步去噪到 t=1
    # 注意：DDPM 的时间步范围是 [0, T-1]，所以最大时间步是 T-1
    for t in reversed(range(1, t_max)):
        t_tensor = torch.full((1,), t, device=device, dtype=torch.long)
        
        # 步骤 3：重采样机制 - 每个时间步重复 U 次
        for u in range(jump_n_sample):
            # 3.1 对未知区域（mask=1）：使用模型反向采样一步，传入类别标签
            img_denoised = sample_timestep(model, img, t_tensor, class_label)
            
            # 3.2 对已知区域（mask=0）：使用原始图像 x_0 前向扩散到 t-1 步
            # 核心公式：x_{t-1}^{known} ~ N(√ᾱ_{t-1} * x_0, (1-ᾱ_{t-1}) * I)
            t_minus_1_tensor = torch.full((1,), t - 1, device=device, dtype=torch.long)
            img_known_noisy, _ = forward_diffusion_sample(original_img, t_minus_1_tensor, device)
            
            # 3.3 合并已知和未知区域
            # 核心公式：x_{t-1} = m ⊙ x_{t-1}^{unknown} + (1-m) ⊙ x_{t-1}^{known}
            img = mask * img_denoised + (1 - mask) * img_known_noisy
            
            # 3.4 如果不是最后一次重采样，前向扩散跳步
            if u < jump_n_sample - 1:
                # 前向扩散 jump_length 步
                jump_steps = min(jump_length, t - 1)
                if jump_steps > 0:
                    for _ in range(jump_steps):
                        t_jump = torch.full((1,), t, device=device, dtype=torch.long)
                        img, _ = forward_diffusion_sample(img, t_jump, device)
    
    # 步骤 4：最后一步 t=0，直接使用模型预测，传入类别标签
    t_zero = torch.full((1,), 0, device=device, dtype=torch.long)
    img = sample_timestep(model, img, t_zero, class_label)
    
    return img

# TODO: 你需要在这个函数中完成模型以及其他相关资源的加载，并调用inpaint进行图像补全，以生成图片
def test_image_inpainting():
    # 设置设备
    device = "cuda" if torch.cuda.is_available() else "cpu"
    print(f"Using device: {device}")
    
    # 加载训练好的模型（使用最佳模型）
    model = SimpleUnet().to(device)
    model.load_state_dict(torch.load(os.path.join(MODEL_DIR, args.model_name), map_location=device, weights_only=True))
    model.eval()
    
    # 设置参数
    img_size = args.img_size
    T = args.T        # 完整的时间步数
    t_max = T      # 从纯噪声 T 开始补全到 0
    jump_length = 10   # 跳跃长度
    jump_n_sample = 10  # 每个时间步的重采样次数
    target_class = args.target_class  # 目标类别
    
    # 准备类别标签张量（如果指定了目标类别）
    class_label = None
    if target_class is not None:
        class_label = torch.full((1,), target_class, device=device, dtype=torch.long)
        print(f"Inpainting with class condition: {target_class}")
    
    # 加载真实图像进行补全（指定img_size以匹配mask大小）
    dataloader = load_transformed_dataset(img_size=img_size, batch_size=1, dataset_name=args.dataset, shuffle=False)
    
    # 获取对应类别的图像作为测试
    if target_class is not None:
        print(f"Loading image from class {target_class}...")
        found = False
        for batch, label in dataloader:
            if label.item() == target_class:
                original_img = batch.to(device)
                found = True
                print(f"Found image from class {target_class}")
                break
        if not found:
            print(f"Warning: No image found for class {target_class}, using first image instead")
            for batch, _ in dataloader:
                original_img = batch.to(device)
                break
    else:
        # 如果没有指定类别，获取第一张图像作为测试
        for batch, _ in dataloader:
            original_img = batch.to(device)
            break
    
    # 创建掩码：中心区域需要补全（值为1），边缘区域保留（值为0）
    mask = torch.zeros((1, 1, img_size, img_size), device=device)
    
    # 创建矩形掩码（中心32x32区域）
    mask_size = 32
    start = (img_size - mask_size) // 2
    end = start + mask_size
    mask[:, :, start:end, start:end] = 1.0
    
    # 生成文件名（根据是否指定类别）
    if target_class is not None:
        original_filename = f'original_image_class_{target_class}.png'
        before_filename = f'image_before_inpainting_class_{target_class}.png'
        inpainted_filename = f'inpainted_image_repaint_class_{target_class}.png'
        comparison_filename = f'comparison_class_{target_class}.png'
        title_suffix = f'(Class {target_class})'
    else:
        original_filename = 'original_image.png'
        before_filename = 'image_before_inpainting.png'
        inpainted_filename = 'inpainted_image_repaint.png'
        comparison_filename = 'comparison.png'
        title_suffix = ''
    
    # 保存原始图像到output目录
    plt.figure(figsize=(8, 8))
    show_tensor_image(original_img)
    plt.axis('off')
    plt.title(f'Original Image {title_suffix}')
    output_path = os.path.join(output_dir, original_filename)
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()
    print(f"Original image saved to: {output_path}")
    
    # 创建待补全的图像用于可视化：将mask区域设置为噪声
    img_for_visualization = original_img.clone()
    t_tensor = torch.full((1,), 150, device=device, dtype=torch.long)
    img_noisy, _ = forward_diffusion_sample(img_for_visualization, t_tensor, device)
    img_for_visualization = mask * img_noisy + (1 - mask) * original_img
    
    # 保存待补全的图像到output目录（仅用于可视化）
    plt.figure(figsize=(8, 8))
    show_tensor_image(img_for_visualization)
    plt.axis('off')
    plt.title(f'Image with Missing Region (Before Inpainting) {title_suffix}')
    output_path = os.path.join(output_dir, before_filename)
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()
    print(f"Image before inpainting saved to: {output_path}")
    
    # 保存掩码可视化到output目录
    plt.figure(figsize=(8, 8))
    plt.imshow(mask[0, 0].cpu().numpy(), cmap='gray')
    plt.axis('off')
    plt.title('Mask (White=Inpaint Region, Black=Keep)')
    output_path = os.path.join(output_dir, 'mask.png')
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()
    print(f"Mask saved to: {output_path}")
    
    # 进行图像补全（传入原始图像，函数内部会从纯噪声初始化）
    print(f"Starting image inpainting with RePaint method...")
    print(f"Parameters: T={T}, t_max={t_max}, jump_length={jump_length}, jump_n_sample={jump_n_sample}")
    inpainted_img = inpaint(model, device, original_img, mask, t_max=t_max, jump_length=jump_length, jump_n_sample=jump_n_sample, class_label=class_label)
    print("Image inpainting completed!")
    
    # 保存补全后的图片到output目录
    plt.figure(figsize=(8, 8))
    show_tensor_image(inpainted_img)
    plt.axis('off')
    plt.title(f'Inpainted Image (RePaint) {title_suffix}')
    output_path = os.path.join(output_dir, inpainted_filename)
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()
    print(f"Inpainted image saved to: {output_path}")
    
    # 对比图：原始图像 vs 补全图像
    fig, axes = plt.subplots(1, 2, figsize=(16, 8))
    
    # 转换原始图像
    original_img_np = ((original_img[0].cpu() + 1) / 2 * 255).permute(1, 2, 0).numpy().astype(np.uint8)
    axes[0].imshow(original_img_np)
    axes[0].axis('off')
    axes[0].set_title(f'Original Image {title_suffix}')
    
    # 转换补全图像
    inpainted_img_np = ((inpainted_img[0].cpu() + 1) / 2 * 255).permute(1, 2, 0).numpy().astype(np.uint8)
    axes[1].imshow(inpainted_img_np)
    axes[1].axis('off')
    axes[1].set_title(f'Inpainted Image {title_suffix}')
    
    plt.tight_layout()
    output_path = os.path.join(output_dir, comparison_filename)
    plt.savefig(output_path, bbox_inches='tight', pad_inches=0)
    plt.close()
    print(f"Comparison image saved to: {output_path}")
    
    return inpainted_img
    

if __name__ == "__main__":
    # 如果使用datasets-2且未指定target_class，则生成两个类别的图像
    if args.dataset == 'datasets-2' and args.target_class is None:
        print("Generating images for both classes...")
        
        # 生成类别0的图像
        args.target_class = 0
        test_image_generation()
        test_image_inpainting()
        
        # 生成类别1的图像
        args.target_class = 1
        test_image_generation()
        test_image_inpainting()
    else:
        # 否则按指定生成
        test_image_generation()
        test_image_inpainting()