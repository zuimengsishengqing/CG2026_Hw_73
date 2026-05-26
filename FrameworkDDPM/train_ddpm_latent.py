from vae import VAE
from dataloader import load_transformed_dataset
from forward_noising import forward_diffusion_sample
import torch
import torch.nn as nn
import torch.nn.functional as F
from torch.optim import Adam
from torch.optim.lr_scheduler import ReduceLROnPlateau
import logging
import os
import argparse
import math
from tqdm import tqdm
import signal
import sys

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')


def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='Latent Space DDPM Training Script')
    
    # 数据集相关参数
    parser.add_argument('--dataset', type=str, default='datasets-3',
                        choices=['datasets-2', 'datasets-3'],
                        help='选择训练数据集')
    
    # VAE相关参数
    parser.add_argument('--vae_path', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model\best_vae.pth',
                        help='VAE模型路径')
    parser.add_argument('--latent_channels', type=int, default=4,
                        help='潜在空间通道数')
    parser.add_argument('--num_classes', type=int, default=2,
                        help='类别数量')
    
    # DDPM相关参数
    parser.add_argument('--T', type=int, default=300,
                        help='扩散时间步数')
    
    # 训练相关参数
    parser.add_argument('--batch_size', type=int, default=8,
                        help='批次大小')
    parser.add_argument('--epochs', type=int, default=5000,
                        help='训练轮数')
    parser.add_argument('--img_size', type=int, default=64,
                        help='输入图像大小')
    parser.add_argument('--lr', type=float, default=5e-4,
                        help='学习率（MLP架构需要稍大的学习率）')
    
    # 模型保存相关参数
    parser.add_argument('--model_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                        help='模型保存目录路径')
    parser.add_argument('--save_interval', type=int, default=500,
                        help='每多少个epoch保存一次检查点')
    parser.add_argument('--resume', type=str, default=None,
                        help='从指定检查点恢复训练 (checkpoint文件名)')
    
    return parser.parse_args()


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
            self.norm1 = nn.GroupNorm(1, out_ch)  # 使用 GroupNorm(1) 等价于 LayerNorm，但更灵活
            self.norm2 = nn.GroupNorm(1, out_ch)
        else:
            self.norm1 = nn.GroupNorm(num_groups, out_ch)
            self.norm2 = nn.GroupNorm(num_groups, out_ch)
        
        # 残差连接：如果输入输出通道数不同，使用 1x1 卷积调整
        if in_ch != out_ch:
            self.residual_conv = nn.Sequential(
                nn.Conv2d(in_ch, out_ch, 1),
                nn.GroupNorm(min(8, out_ch), out_ch),  # 添加归一化
                nn.SiLU()  # 添加激活函数
            )
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


def safe_save_model(state_dict, filepath, max_retries=3, delay=1.0):
    """
    安全保存模型，使用临时文件+重命名的方式确保原子性操作
    处理文件占用问题，支持重试机制
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


def save_checkpoint(model, optimizer, epoch, best_loss, device, model_dir, filename='latent_ddpm_checkpoint.pth', max_retries=3):
    """
    保存训练检查点，包括模型、优化器状态和训练进度
    """
    checkpoint = {
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'epoch': epoch,
        'best_loss': best_loss,
        'device': device
    }
    
    checkpoint_path = os.path.join(model_dir, filename)
    
    # 使用安全保存机制
    return safe_save_model(checkpoint, checkpoint_path, max_retries=max_retries)


def find_latest_checkpoint(model_dir):
    """
    查找最新的检查点文件
    优先级：interrupted > epoch_N > checkpoint > best
    """
    import re
    
    # 检查中断检查点
    interrupted_path = os.path.join(model_dir, 'latent_ddpm_interrupted.pth')
    if os.path.exists(interrupted_path):
        return interrupted_path, 'interrupted'
    
    # 查找所有epoch检查点
    epoch_checkpoints = []
    for filename in os.listdir(model_dir):
        if filename.startswith('latent_ddpm_epoch_') and filename.endswith('.pth'):
            match = re.search(r'epoch_(\d+)', filename)
            if match:
                epoch_num = int(match.group(1))
                epoch_checkpoints.append((epoch_num, filename))
    
    # 按epoch编号排序，取最新的
    if epoch_checkpoints:
        epoch_checkpoints.sort(key=lambda x: x[0], reverse=True)
        latest_epoch = epoch_checkpoints[0]
        return os.path.join(model_dir, latest_epoch[1]), f'epoch_{latest_epoch[0]}'
    
    # 检查常规检查点
    checkpoint_path = os.path.join(model_dir, 'latent_ddpm_checkpoint.pth')
    if os.path.exists(checkpoint_path):
        return checkpoint_path, 'checkpoint'
    
    # 检查best检查点
    best_path = os.path.join(model_dir, 'best_latent_ddpm.pth')
    if os.path.exists(best_path):
        return best_path, 'best'
    
    return None, None


def load_checkpoint(model, optimizer, model_dir, filename='latent_ddpm_checkpoint.pth', user_lr=None):
    """
    加载训练检查点，恢复训练状态
    """
    checkpoint_path = os.path.join(model_dir, filename)
    if os.path.exists(checkpoint_path):
        checkpoint = torch.load(checkpoint_path)
        model.load_state_dict(checkpoint['model_state_dict'])
        optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        epoch = checkpoint['epoch']
        best_loss = checkpoint['best_loss']
        device = checkpoint['device']
        
        # 如果用户指定了学习率，覆盖检查点中的学习率
        if user_lr is not None:
            for param_group in optimizer.param_groups:
                param_group['lr'] = user_lr
            logging.info(f"Learning rate overridden from checkpoint: {user_lr:.6f}")
        
        logging.info(f"Checkpoint loaded from {checkpoint_path}: epoch {epoch}, best_loss {best_loss:.6f}")
        return epoch, best_loss, device
    else:
        logging.info(f"No checkpoint found at {checkpoint_path}, starting from scratch")
        return 0, float('inf'), None


# 全局变量用于保存训练状态
TRAINING_STATE = {
    'model': None,
    'optimizer': None,
    'epoch': 0,
    'best_loss': float('inf'),
    'device': None,
    'model_dir': None
}


def signal_handler(sig, frame):
    """
    处理Ctrl+C信号，保存训练状态后退出
    """
    logging.info("\nTraining interrupted by user. Saving checkpoint...")
    if TRAINING_STATE['model'] is not None and TRAINING_STATE['optimizer'] is not None:
        model_dir = TRAINING_STATE.get('model_dir', '.')
        save_checkpoint(
            TRAINING_STATE['model'],
            TRAINING_STATE['optimizer'],
            TRAINING_STATE['epoch'],
            TRAINING_STATE['best_loss'],
            TRAINING_STATE['device'],
            model_dir,
            filename=os.path.join(model_dir, 'latent_ddpm_interrupted.pth')
        )
    sys.exit(0)


# 注册信号处理器
signal.signal(signal.SIGINT, signal_handler)


class LatentUNet(nn.Module):
    def __init__(self, latent_channels=4, num_classes=2):
        super().__init__()
        time_emb_dim = 32
        
        # 时间嵌入
        self.time_mlp = nn.Sequential(
            SinusoidalPositionEmbeddings(time_emb_dim),
            nn.Linear(time_emb_dim, time_emb_dim),
            nn.SiLU()  # 使用 SiLU 激活函数
        )
        
        # 类别嵌入
        self.class_emb = nn.Embedding(num_classes, time_emb_dim)
        
        # 重要优化 2：对于 4x4 的特征图，不应该再下采样
        # 否则会丢失空间信息，导致模型无法还原任何结构
        # 改为不进行下采样和上采样，只使用中间层
        
        # 第一层：增加通道数，保持空间尺寸
        self.block1 = LatentBlock(latent_channels, 64, time_emb_dim)
        
        # 第二层：进一步增加通道数，保持空间尺寸
        self.block2 = LatentBlock(64, 128, time_emb_dim)
        
        # 中间层：不改变空间尺寸，只增加深度
        self.mid1 = LatentBlock(128, 128, time_emb_dim)
        self.mid2 = LatentBlock(128, 128, time_emb_dim)
        
        # 第三层：减少通道数（注意：输入通道数是 128 + 128 = 256，因为要拼接 h2）
        self.block3 = LatentBlock(128 + 128, 64, time_emb_dim)
        
        # 输出层：恢复到原始通道数（注意：输入通道数是 64 + 64 = 128，因为要拼接 h1）
        self.block4 = LatentBlock(64 + 64, latent_channels, time_emb_dim)
    
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
        
        # 重要优化 2：新的前向传播，保持 4x4 的空间尺寸
        # 不再进行下采样和上采样，避免空间维度塌陷
        
        # 编码器（保存特征用于跳跃连接）
        h1 = self.block1(x, t_emb)  # (batch, 64, 4, 4)
        h2 = self.block2(h1, t_emb)  # (batch, 128, 4, 4)
        
        # 中间层：不改变空间尺寸，只增加深度
        h = self.mid1(h2, t_emb)  # (batch, 128, 4, 4)
        h = self.mid2(h, t_emb)  # (batch, 128, 4, 4)
        
        # 解码器 + 跳跃连接（关键修复：U-Net 的核心）
        # 使用通道维度拼接（Concat）而不是逐元素相加，完整保留编码器的多维度特征
        h = self.block3(torch.cat([h, h2], dim=1), t_emb)  # (batch, 64, 4, 4)
        h = self.block4(torch.cat([h, h1], dim=1), t_emb)  # (batch, 4, 4, 4)
        
        return h


# 无分类器引导（CFG）的丢弃概率
CFG_DROPOUT_PROB = 0.1  # 10% 的概率丢弃类别标签


def get_loss(model, z_0, t, device, class_label=None):
    """
    计算潜在空间DDPM的损失（支持无分类器引导 CFG）
    """
    # 前向加噪：对潜在向量加噪
    z_noisy, noise = forward_diffusion_sample(z_0, t, device)
    
    # 无分类器引导（CFG）：随机丢弃类别标签
    if class_label is not None:
        cfg_mask = torch.rand(class_label.shape[0], device=device) > CFG_DROPOUT_PROB
        class_label_for_model = class_label.clone()
        class_label_for_model[~cfg_mask] = -1
    else:
        class_label_for_model = None
    
    # 模型预测噪声
    predicted_noise = model(z_noisy, t, class_label_for_model)
    
    # 计算MSE损失
    loss = F.mse_loss(noise, predicted_noise)
    
    # 计算详细的loss信息
    loss_dict = {
        'total_loss': loss.item(),
        'noise_mse': F.mse_loss(noise, predicted_noise).item()
    }
    
    # 按时间步分类损失
    t_normalized = t.float() / 300
    loss_dict['t_normalized'] = t_normalized.mean().item()
    
    # 按类别分类损失
    if class_label is not None:
        unique_classes = torch.unique(class_label)
        for cls in unique_classes:
            mask = (class_label == cls)
            if mask.sum() > 0:
                cls_loss = F.mse_loss(noise[mask], predicted_noise[mask])
                loss_dict[f'class_{cls.item()}_loss'] = cls_loss.item()
    
    return loss, loss_dict


def train_latent_ddpm():
    args = parse_args()
    
    # 模型存储目录配置
    MODEL_DIR = args.model_dir
    os.makedirs(MODEL_DIR, exist_ok=True)
    logging.info(f"Model directory: {MODEL_DIR}")
    logging.info(f"Dataset: {args.dataset}")
    logging.info(f"Batch size: {args.batch_size}")
    logging.info(f"Epochs: {args.epochs}")
    logging.info(f"Time steps (T): {args.T}")
    logging.info(f"Latent channels: {args.latent_channels}")
    logging.info(f"Learning rate: {args.lr}")
    
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
    
    # 加载预训练的VAE（固定参数）
    vae = VAE(
        in_channels=3, 
        latent_channels=args.latent_channels, 
        num_classes=args.num_classes
    ).to(device)
    
    vae.load_state_dict(torch.load(args.vae_path, map_location=device, weights_only=True))
    vae.eval()
    for param in vae.parameters():
        param.requires_grad = False
    
    logging.info(f"VAE loaded from {args.vae_path} (frozen)")
    
    # 重要优化 1：计算潜空间的标准差，并进行缩放
    # 这是 DDPM 能够收敛的关键！
    logging.info("Computing latent space statistics...")
    latent_stds = []
    with torch.no_grad():
        for batch, class_labels in dataloader:
            batch = batch.to(device)
            class_labels = class_labels.to(device)
            z = vae.encode_mu(batch, class_labels)
            latent_stds.append(z.std().item())
    
    avg_latent_std = sum(latent_stds) / len(latent_stds)
    logging.info(f"Average latent std: {avg_latent_std:.4f}")
    
    # 计算缩放因子，使潜空间方差接近 1
    # scale_factor = 1.0 / avg_latent_std
    # 这样缩放后的潜空间方差约为 1
    scale_factor = 1.0 / avg_latent_std
    logging.info(f"Latent scaling factor: {scale_factor:.4f}")
    
    # 初始化潜在空间DDPM模型
    model = LatentUNet(latent_channels=args.latent_channels, num_classes=args.num_classes).to(device)
    optimizer = Adam(model.parameters(), lr=args.lr)
    
    # 学习率调度器：当loss停滞时降低学习率
    scheduler = ReduceLROnPlateau(
        optimizer, 
        mode='min', 
        factor=0.5,  # 每次降低50%
        patience=50,  # 50个epoch没有改善就降低学习率
        min_lr=1e-6  # 最小学习率
    )
    logging.info(f"Learning rate scheduler: ReduceLROnPlateau (factor=0.5, patience=50)")
    
    logging.info(f"Latent DDPM parameters: {sum(p.numel() for p in model.parameters())}")
    
    # 尝试加载检查点（恢复训练）
    if args.resume:
        checkpoint_file = args.resume
        start_epoch, best_loss, loaded_device = load_checkpoint(model, optimizer, MODEL_DIR, checkpoint_file, user_lr=args.lr)
    else:
        latest_checkpoint, checkpoint_type = find_latest_checkpoint(MODEL_DIR)
        if latest_checkpoint:
            logging.info(f"Found latest checkpoint: {os.path.basename(latest_checkpoint)} (type: {checkpoint_type})")
            start_epoch, best_loss, loaded_device = load_checkpoint(model, optimizer, MODEL_DIR, os.path.basename(latest_checkpoint), user_lr=args.lr)
        else:
            logging.info("No checkpoint found, starting from scratch")
            start_epoch, best_loss, loaded_device = 0, float('inf'), None
    
    # 如果加载了检查点，使用加载的设备
    if loaded_device is not None:
        device = loaded_device
        model.to(device)
        logging.info(f"Resuming training on device: {device}")
    
    # 更新全局训练状态
    TRAINING_STATE['model'] = model
    TRAINING_STATE['optimizer'] = optimizer
    TRAINING_STATE['device'] = device
    TRAINING_STATE['best_loss'] = best_loss
    TRAINING_STATE['model_dir'] = MODEL_DIR
    
    # 训练循环
    logging.info("="*50)
    logging.info("Starting training...")
    logging.info("="*50)
    
    for epoch in range(start_epoch, args.epochs):
        # 立即更新全局训练状态，确保Ctrl+C时保存正确的epoch
        TRAINING_STATE['epoch'] = epoch
        
        model.train()
        epoch_loss = 0.0
        epoch_loss_dict = {
            'total_loss': 0.0,
            'noise_mse': 0.0,
            't_normalized': 0.0,
            'class_0_loss': 0.0,
            'class_1_loss': 0.0,
            'class_0_count': 0,
            'class_1_count': 0
        }
        
        # 使用tqdm显示进度条
        pbar = tqdm(enumerate(dataloader), total=num_batches, desc=f"Epoch {epoch}/{args.epochs}")
        
        for batch_idx, (batch, class_labels) in pbar:
            # 数据移动到设备
            batch = batch.to(device)
            class_labels = class_labels.to(device)
            
            # 清零梯度
            optimizer.zero_grad()
            
            # 使用VAE编码器将图像编码为潜在向量
            with torch.no_grad():
                z_0 = vae.encode_mu(batch, class_labels)
                # 重要优化 1：对潜变量进行缩放，使其方差接近 1
                z_0 = z_0 * scale_factor
            
            # 时间步采样
            t = torch.randint(0, args.T, (batch.shape[0],), device=device).long()
            
            # 计算损失
            loss, loss_dict = get_loss(model, z_0, t, device, class_labels)
            
            # 反向传播
            loss.backward()
            
            # 梯度裁剪：防止梯度爆炸
            torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)
            
            optimizer.step()
            
            # 累积损失
            epoch_loss += loss_dict['total_loss']
            epoch_loss_dict['total_loss'] += loss_dict['total_loss']
            epoch_loss_dict['noise_mse'] += loss_dict['noise_mse']
            epoch_loss_dict['t_normalized'] += loss_dict['t_normalized']
            
            # 按类别累积损失
            if 'class_0_loss' in loss_dict:
                epoch_loss_dict['class_0_loss'] += loss_dict['class_0_loss']
                epoch_loss_dict['class_0_count'] += 1
            if 'class_1_loss' in loss_dict:
                epoch_loss_dict['class_1_loss'] += loss_dict['class_1_loss']
                epoch_loss_dict['class_1_count'] += 1
            
            # 更新进度条
            pbar.set_postfix({
                'loss': f'{loss.item():.4f}',
                't': f'{loss_dict["t_normalized"]:.2f}'
            })
        
        # 计算平均损失
        avg_loss = epoch_loss / num_batches
        avg_noise_mse = epoch_loss_dict['noise_mse'] / num_batches
        avg_t_normalized = epoch_loss_dict['t_normalized'] / num_batches
        
        # 计算各类别平均损失
        avg_class_0_loss = epoch_loss_dict['class_0_loss'] / epoch_loss_dict['class_0_count'] if epoch_loss_dict['class_0_count'] > 0 else 0
        avg_class_1_loss = epoch_loss_dict['class_1_loss'] / epoch_loss_dict['class_1_count'] if epoch_loss_dict['class_1_count'] > 0 else 0
        
        # 更新学习率调度器
        scheduler.step(avg_loss)
        current_lr = optimizer.param_groups[0]['lr']
        
        logging.info(
            f"Epoch {epoch}/{args.epochs} | "
            f"Total Loss: {avg_loss:.6f} | "
            f"Noise MSE: {avg_noise_mse:.6f} | "
            f"Avg T: {avg_t_normalized:.3f} | "
            f"Class 0 Loss: {avg_class_0_loss:.6f} | "
            f"Class 1 Loss: {avg_class_1_loss:.6f} | "
            f"LR: {current_lr:.6f} | "
            f"Best Loss: {best_loss:.6f}"
        )
        
        # 更新全局训练状态中的best_loss
        TRAINING_STATE['best_loss'] = best_loss
        
        # 保存最佳模型
        if avg_loss < best_loss:
            best_loss = avg_loss
            TRAINING_STATE['best_loss'] = best_loss
            safe_save_model(
                model.state_dict(), 
                os.path.join(MODEL_DIR, 'best_latent_ddpm.pth'),
                max_retries=3
            )
            logging.info(f"✓ New best latent DDPM saved with loss: {best_loss:.6f}")
        
        # 定期保存检查点
        if (epoch + 1) % args.save_interval == 0:
            save_checkpoint(
                model, optimizer, epoch + 1, best_loss, device, MODEL_DIR,
                filename=os.path.join(MODEL_DIR, f'latent_ddpm_epoch_{epoch + 1}.pth')
            )
            # 同时更新主检查点
            save_checkpoint(
                model, optimizer, epoch + 1, best_loss, device, MODEL_DIR,
                filename=os.path.join(MODEL_DIR, 'latent_ddpm_checkpoint.pth')
            )
    
    # 训练完成，保存最终模型
    safe_save_model(
        model.state_dict(), 
        os.path.join(MODEL_DIR, 'final_latent_ddpm.pth'),
        max_retries=3
    )
    
    # 保存最终检查点
    save_checkpoint(
        model, optimizer, args.epochs, best_loss, device, MODEL_DIR,
        filename=os.path.join(MODEL_DIR, 'latent_ddpm_final_checkpoint.pth')
    )
    
    # 删除中断检查点（如果存在）
    interrupted_checkpoint = os.path.join(MODEL_DIR, 'latent_ddpm_interrupted.pth')
    if os.path.exists(interrupted_checkpoint):
        try:
            os.remove(interrupted_checkpoint)
            logging.info(f"✓ Removed interrupted checkpoint: {interrupted_checkpoint}")
        except Exception as e:
            logging.warning(f"Failed to remove interrupted checkpoint: {e}")
    
    logging.info(f"Training completed! Best loss: {best_loss:.6f}")


if __name__ == "__main__":
    train_latent_ddpm()