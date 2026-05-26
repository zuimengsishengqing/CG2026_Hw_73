
from vae import VAE, vae_loss_function
from dataloader import load_transformed_dataset
import torch
from torch.optim import Adam
import logging
import os
import argparse
import numpy as np
from tqdm import tqdm
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from dataloader import show_tensor_image

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='VAE Training Script')
    
    # 数据集相关参数
    parser.add_argument('--dataset', type=str, default='datasets-3',
                        choices=['datasets-2', 'datasets-3'],
                        help='选择训练数据集')
    
    # 模型相关参数
    parser.add_argument('--latent_channels', type=int, default=4,
                        help='潜在空间通道数')
    parser.add_argument('--num_classes', type=int, default=2,
                        help='类别数量')
    
    # 训练相关参数
    parser.add_argument('--batch_size', type=int, default=8,
                        help='批次大小')
    parser.add_argument('--epochs', type=int, default=100,
                        help='训练轮数')
    parser.add_argument('--img_size', type=int, default=64,
                        help='输入图像大小')
    parser.add_argument('--lr', type=float, default=1e-4,
                        help='学习率')
    parser.add_argument('--beta', type=float, default=0.1,
                        help='KL散度权重系数（默认0.1，让KL散度与重建损失在同一个数量级）')
    parser.add_argument('--kl_annealing', action='store_true',
                        help='是否使用KL退火策略')
    parser.add_argument('--kl_anneal_epochs', type=int, default=50,
                        help='KL退火的轮数（前N个epoch逐渐增加KL权重）')
    parser.add_argument('--use_l1_loss', action='store_true',
                        help='是否使用 L1 损失替代 MSE（L1 更能保留高频细节和锐利边缘）')
    parser.add_argument('--compute_scaling', action='store_true',
                        help='是否在训练前计算缩放因子')
    parser.add_argument('--scaling_samples', type=int, default=1000,
                        help='计算缩放因子时的采样数量')
    
    # 模型保存相关参数
    parser.add_argument('--model_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                        help='模型保存目录路径')
    parser.add_argument('--save_interval', type=int, default=500,
                        help='每多少个epoch保存一次检查点')
    
    # 数据增强参数
    parser.add_argument('--use_augmentation', action='store_true',
                        help='是否使用数据增强')
    
    return parser.parse_args()


def safe_save_model(state_dict, filepath, max_retries=3, delay=0.5):
    """
    安全保存模型，处理文件占用问题
    使用临时文件+重命名的方式确保原子性操作
    """
    import time
    import tempfile
    import shutil
    
    for attempt in range(max_retries):
        try:
            # 创建临时文件
            temp_dir = os.path.dirname(filepath)
            with tempfile.NamedTemporaryFile(mode='wb', dir=temp_dir, delete=False, suffix='.tmp') as tmp_file:
                temp_path = tmp_file.name
            
            # 保存到临时文件
            torch.save(state_dict, temp_path)
            
            # 重命名到目标文件（原子操作）
            if os.path.exists(filepath):
                # 先删除旧文件（Windows可能需要重试）
                for retry in range(3):
                    try:
                        os.remove(filepath)
                        break
                    except PermissionError:
                        if retry < 2:
                            time.sleep(delay)
                        else:
                            raise
            
            # 重命名临时文件为目标文件
            os.rename(temp_path, filepath)
            
            logging.info(f"✓ Model saved successfully to {filepath}")
            return True
            
        except (PermissionError, OSError, RuntimeError) as e:
            logging.warning(f"Save attempt {attempt + 1}/{max_retries} failed: {e}")
            if attempt < max_retries - 1:
                time.sleep(delay)
                # 清理可能残留的临时文件
                if 'temp_path' in locals() and os.path.exists(temp_path):
                    try:
                        os.remove(temp_path)
                    except:
                        pass
            else:
                logging.error(f"Failed to save model after {max_retries} attempts: {filepath}")
                return False
    
    return False


def train_vae():
    args = parse_args()
    
    # 模型存储目录配置
    MODEL_DIR = args.model_dir
    os.makedirs(MODEL_DIR, exist_ok=True)
    logging.info(f"Model directory: {MODEL_DIR}")
    logging.info(f"Dataset: {args.dataset}")
    logging.info(f"Batch size: {args.batch_size}")
    logging.info(f"Epochs: {args.epochs}")
    logging.info(f"Latent channels: {args.latent_channels}")
    logging.info(f"Learning rate: {args.lr}")
    logging.info(f"KL weight (beta): {args.beta}")
    logging.info(f"KL annealing: {args.kl_annealing}")
    if args.kl_annealing:
        logging.info(f"KL anneal epochs: {args.kl_anneal_epochs}")
    
    # 数据加载
    dataloader = load_transformed_dataset(
        img_size=args.img_size, 
        batch_size=args.batch_size, 
        dataset_name=args.dataset
    )
    
    # 打印数据集信息
    dataset_size = len(dataloader.dataset)
    num_batches = len(dataloader)
    logging.info(f"Dataset size: {dataset_size} images")
    logging.info(f"Batch size: {args.batch_size}")
    logging.info(f"Batches per epoch: {num_batches}")
    
    # 设备配置
    device = "cuda" if torch.cuda.is_available() else "cpu"
    if torch.cuda.is_available():
        logging.info(f"CUDA available: {torch.cuda.is_available()}")
        logging.info(f"GPU name: {torch.cuda.get_device_name(0)}")
    logging.info(f"Using device: {device}")
    
    # 模型和优化器初始化
    vae = VAE(
        in_channels=3, 
        latent_channels=args.latent_channels, 
        num_classes=args.num_classes
    ).to(device)
    
    optimizer = Adam(vae.parameters(), lr=args.lr)
    
    logging.info(f"VAE parameters: {sum(p.numel() for p in vae.parameters())}")
    
    # 计算潜在空间缩放因子（可选）
    if args.compute_scaling:
        logging.info("="*50)
        logging.info("Computing latent space scaling factor...")
        logging.info("="*50)
        
        # 先加载一个预训练模型（如果存在）来计算缩放因子
        vae_path = os.path.join(MODEL_DIR, 'best_vae.pth')
        if os.path.exists(vae_path):
            vae.load_state_dict(torch.load(vae_path, map_location=device))
            logging.info(f"Loaded existing VAE from {vae_path}")
        
        # 计算缩放因子
        scaling_factor = vae.compute_scaling_factor(dataloader, device, args.scaling_samples)
        # scaling_factor现在是nn.Parameter，需要使用.data.fill_()设置值
        vae.scaling_factor.data.fill_(scaling_factor)
        logging.info(f"Scaling factor set to: {vae.scaling_factor.item():.6f}")
        logging.info("="*50)
    
    # 训练循环
    best_loss = float('inf')
    
    logging.info("="*50)
    logging.info("Starting training...")
    logging.info("="*50)
    
    for epoch in range(args.epochs):
        vae.train()
        epoch_recon_loss = 0.0
        epoch_kl_loss = 0.0
        epoch_total_loss = 0.0
        
        # KL退火策略：动态调整beta值
        if args.kl_annealing:
            # 前期专注于重建，后期逐渐增加KL权重
            if epoch < args.kl_anneal_epochs:
                current_beta = args.beta * (epoch / args.kl_anneal_epochs)
            else:
                current_beta = args.beta
        else:
            current_beta = args.beta
        
        # 使用tqdm显示进度条
        pbar = tqdm(enumerate(dataloader), total=num_batches, desc=f"Epoch {epoch}/{args.epochs}")
        
        for batch_idx, (batch, class_labels) in pbar:
            # 数据移动到设备
            batch = batch.to(device)
            class_labels = class_labels.to(device)
            
            # 清零梯度
            optimizer.zero_grad()
            
            # 前向传播
            x_recon, mu, logvar, z = vae(batch, class_labels)
            
            # 计算损失（使用动态的beta值）
            total_loss, recon_loss, kl_loss = vae_loss_function(
                x_recon, batch, mu, logvar, beta=current_beta, use_l1_loss=args.use_l1_loss
            )
            
            # 反向传播
            total_loss.backward()
            
            # 梯度裁剪：防止梯度爆炸，让 Loss 下降更加平滑稳定
            torch.nn.utils.clip_grad_norm_(vae.parameters(), max_norm=1.0)
            
            optimizer.step()
            
            # 累积损失
            epoch_recon_loss += recon_loss.item()
            epoch_kl_loss += kl_loss.item()
            epoch_total_loss += total_loss.item()
            
            # 更新进度条
            pbar.set_postfix({
                'total': f'{total_loss.item():.1f}',
                'recon': f'{recon_loss.item():.1f}',
                'kl': f'{kl_loss.item():.1f}',
                'beta': f'{current_beta:.4f}'
            })
        
        # 计算平均损失
        avg_recon_loss = epoch_recon_loss / dataset_size
        avg_kl_loss = epoch_kl_loss / dataset_size
        avg_total_loss = epoch_total_loss / dataset_size
        
        logging.info(
            f"Epoch {epoch}/{args.epochs} | "
            f"Total Loss: {avg_total_loss:.4f} | "
            f"Recon Loss: {avg_recon_loss:.4f} | "
            f"KL Loss: {avg_kl_loss:.4f} | "
            f"Beta: {current_beta:.4f} | "
            f"Best Loss: {best_loss:.4f}"
        )
        
        # 保存最佳模型
        if avg_total_loss < best_loss:
            best_loss = avg_total_loss
            safe_save_model(vae.state_dict(), os.path.join(MODEL_DIR, 'best_vae.pth'))
            logging.info(f"✓ New best VAE saved with loss: {best_loss:.4f}")
        
        # 定期保存检查点
        if (epoch + 1) % args.save_interval == 0:
            safe_save_model(vae.state_dict(), os.path.join(MODEL_DIR, f'vae_epoch_{epoch + 1}.pth'))
            logging.info(f"✓ Checkpoint saved at epoch {epoch + 1}")
    
    # 训练完成，保存最终模型
    safe_save_model(vae.state_dict(), os.path.join(MODEL_DIR, 'final_vae.pth'))
    logging.info(f"Training completed! Best loss: {best_loss:.4f}")
    
    # 生成一些重建图像进行验证
    vae.eval()
    with torch.no_grad():
        for batch, class_labels in dataloader:
            batch = batch.to(device)
            class_labels = class_labels.to(device)
            
            x_recon, _, _, _ = vae(batch, class_labels)
            
            # 保存对比图
            fig, axes = plt.subplots(2, 4, figsize=(16, 8))
            for i in range(4):
                # 原始图像 - 转换为numpy数组
                orig_img = ((batch[i].cpu() + 1) / 2 * 255).permute(1, 2, 0).numpy().astype(np.uint8)
                axes[0, i].imshow(orig_img)
                axes[0, i].axis('off')
                axes[0, i].set_title(f'Original (Class {class_labels[i].item()})')
                
                # 重建图像 - 转换为numpy数组
                recon_img = ((x_recon[i].cpu() + 1) / 2 * 255).permute(1, 2, 0).numpy().astype(np.uint8)
                axes[1, i].imshow(recon_img)
                axes[1, i].axis('off')
                axes[1, i].set_title(f'Reconstructed (Class {class_labels[i].item()})')
            
            plt.tight_layout()
            plt.savefig(os.path.join(MODEL_DIR, 'vae_reconstruction.png'), bbox_inches='tight', pad_inches=0)
            plt.close()
            logging.info(f"Reconstruction images saved to {os.path.join(MODEL_DIR, 'vae_reconstruction.png')}")
            break


if __name__ == "__main__":
    train_vae()