import torch
import torch.nn as nn
from vae import VAE
from train_flow_latent import LatentUNet
from torchdiffeq import odeint  # 论文推荐ODE求解器
import matplotlib.pyplot as plt
from dataloader import show_tensor_image
import os
import argparse
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='Latent OT-CFM Sampling')
    
    # 模型路径参数
    parser.add_argument('--vae_path', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model\best_vae.pth',
                        help='VAE模型路径')
    parser.add_argument('--flow_path', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model\best_flow.pth',
                        help='Flow模型路径')
    parser.add_argument('--checkpoint_path', type=str, default=None,
                        help='检查点路径（如果指定，将从中加载scale等参数）')
    
    # 模型参数
    parser.add_argument('--latent_channels', type=int, default=4,
                        help='潜在空间通道数')
    parser.add_argument('--num_classes', type=int, default=2,
                        help='类别数量')
    
    # 采样参数
    parser.add_argument('--classes', type=int, nargs='+', default=[0, 1],
                        help='要生成的类别列表')
    parser.add_argument('--num_samples', type=int, default=1,
                        help='每个类别生成的样本数')
    parser.add_argument('--img_size', type=int, default=64,
                        help='输出图像大小')
    parser.add_argument('--output_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM',
                        help='输出图像保存目录')
    parser.add_argument('--scale', type=float, default=1.0,
                        help='潜空间缩放因子（如果不从检查点加载）')
    
    return parser.parse_args()

@torch.no_grad()
def ode_sampling_func(t: float, z: torch.Tensor, model, label) -> torch.Tensor:
    """ODE函数：dx/dt = v_t(x)，对标论文公式1"""
    t_tensor = torch.tensor([t], device=z.device).expand(z.shape[0])
    return model(z, t_tensor, label)

@torch.no_grad()
def sample_flow(vae, model, label, scale, device, img_size=64):
    """CFM采样：解ODE，替代DDPM离散步"""
    # 初始噪声z0~N(0,I)
    z = torch.randn(1, 4, 4, 4, device=device)  # 对应你的latent 4x4
    # 解ODE：t从0→1，对标论文6.2节
    z1 = odeint(lambda t, z: ode_sampling_func(t, z, model, label), z, torch.tensor([0.0,1.0], device=device), method='dopri5')[-1]
    # 反缩放+VAE解码
    z1 = z1 / scale
    img = vae.decode(z1, label)
    return img

if __name__ == '__main__':
    args = parse_args()
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    
    logging.info(f"Using device: {device}")
    logging.info(f"Loading VAE from {args.vae_path}")
    logging.info(f"Loading Flow model from {args.flow_path}")
    
    # 加载模型
    vae = VAE(3, args.latent_channels, args.num_classes).to(device).eval()
    vae.load_state_dict(torch.load(args.vae_path, map_location=device, weights_only=True))
    model = LatentUNet(args.latent_channels, args.num_classes).to(device).eval()
    model.load_state_dict(torch.load(args.flow_path, map_location=device, weights_only=True))
    
    # 尝试从检查点加载scale
    scale = args.scale
    if args.checkpoint_path and os.path.exists(args.checkpoint_path):
        checkpoint = torch.load(args.checkpoint_path, map_location=device)
        if 'scale' in checkpoint:
            scale = checkpoint['scale']
            logging.info(f"Loaded scale from checkpoint: {scale:.4f}")
    else:
        logging.info(f"Using scale: {scale:.4f}")
    
    # 生成指定类别的样本
    for cls in args.classes:
        for sample_idx in range(args.num_samples):
            label = torch.tensor([cls], device=device)
            img = sample_flow(vae, model, label, scale, device, args.img_size)
            
            # 保存图像
            if args.num_samples > 1:
                filename = os.path.join(args.output_dir, f'flow_cls{cls}_sample{sample_idx}.png')
            else:
                filename = os.path.join(args.output_dir, f'flow_cls{cls}.png')
            
            plt.figure()
            show_tensor_image(img)
            plt.savefig(filename)
            plt.close()
            logging.info(f"Saved sample to {filename}")