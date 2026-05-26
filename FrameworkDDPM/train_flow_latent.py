from vae import VAE
from dataloader import load_transformed_dataset
from cfm import sample_continuous_t, ot_path, cfm_loss
import torch
import torch.nn as nn
import math
from torch.optim import Adam
from torch.optim.lr_scheduler import ReduceLROnPlateau
from tqdm import tqdm
import signal
import sys
import os
import logging
import argparse
import time
import tempfile

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='Latent OT-CFM Training')
    
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
    
    # 训练相关参数
    parser.add_argument('--batch_size', type=int, default=16,
                        help='批次大小')
    parser.add_argument('--epochs', type=int, default=2000,
                        help='训练轮数')
    parser.add_argument('--img_size', type=int, default=64,
                        help='输入图像大小')
    parser.add_argument('--lr', type=float, default=1e-4,
                        help='学习率（CFM建议使用较小的学习率）')
    parser.add_argument('--warmup_epochs', type=int, default=100,
                        help='学习率预热轮数')
    parser.add_argument('--grad_clip', type=float, default=0.5,
                        help='梯度裁剪阈值（CFM建议使用较小的阈值）')
    parser.add_argument('--use_ema', action='store_true',
                        help='是否使用指数移动平均（EMA）')
    parser.add_argument('--ema_decay', type=float, default=0.9999,
                        help='EMA衰减系数')
    
    # 模型保存相关参数
    parser.add_argument('--model_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                        help='模型保存目录路径')
    parser.add_argument('--save_interval', type=int, default=500,
                        help='每多少个epoch保存一次检查点')
    parser.add_argument('--resume', type=str, default=None,
                        help='从指定检查点恢复训练 (checkpoint文件名)')
    
    return parser.parse_args()

def safe_save_model(state_dict, filepath, max_retries=3, delay=1.0):
    """
    安全保存模型，使用临时文件+重命名的方式确保原子性操作
    处理文件占用问题，支持重试机制
    """
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

def save_checkpoint(model, optimizer, scheduler, epoch, best_loss, scale, device, model_dir, filename='latent_flow_checkpoint.pth', max_retries=3, ema_model=None):
    """
    保存训练检查点，包括模型、优化器状态和训练进度
    """
    checkpoint = {
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'scheduler_state_dict': scheduler.state_dict(),
        'epoch': epoch,
        'best_loss': best_loss,
        'scale': scale,
        'device': device
    }
    
    # 如果有EMA模型，也保存EMA模型状态
    if ema_model is not None:
        checkpoint['ema_model_state_dict'] = ema_model.state_dict()
    
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
    interrupted_path = os.path.join(model_dir, 'latent_flow_interrupted.pth')
    if os.path.exists(interrupted_path):
        return interrupted_path, 'interrupted'
    
    # 查找所有epoch检查点
    epoch_checkpoints = []
    for filename in os.listdir(model_dir):
        if filename.startswith('latent_flow_epoch_') and filename.endswith('.pth'):
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
    checkpoint_path = os.path.join(model_dir, 'latent_flow_checkpoint.pth')
    if os.path.exists(checkpoint_path):
        return checkpoint_path, 'checkpoint'
    
    # 检查best检查点
    best_path = os.path.join(model_dir, 'best_flow.pth')
    if os.path.exists(best_path):
        return best_path, 'best'
    
    return None, None

def load_checkpoint(model, optimizer, scheduler, model_dir, filename='latent_flow_checkpoint.pth', user_lr=None):
    """
    加载训练检查点，恢复训练状态
    """
    checkpoint_path = os.path.join(model_dir, filename)
    if os.path.exists(checkpoint_path):
        checkpoint = torch.load(checkpoint_path)
        model.load_state_dict(checkpoint['model_state_dict'])
        optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        scheduler.load_state_dict(checkpoint['scheduler_state_dict'])
        epoch = checkpoint['epoch']
        best_loss = checkpoint['best_loss']
        scale = checkpoint['scale']
        device = checkpoint['device']
        
        # 如果用户指定了学习率，覆盖检查点中的学习率
        if user_lr is not None:
            for param_group in optimizer.param_groups:
                param_group['lr'] = user_lr
            logging.info(f"Learning rate overridden from checkpoint: {user_lr:.6f}")
        
        # 检查是否有EMA模型
        ema_state = None
        if 'ema_model_state_dict' in checkpoint:
            ema_state = checkpoint['ema_model_state_dict']
        
        logging.info(f"Checkpoint loaded from {checkpoint_path}: epoch {epoch}, best_loss {best_loss:.6f}, scale {scale:.4f}")
        return epoch, best_loss, scale, device, ema_state
    else:
        logging.info(f"No checkpoint found at {checkpoint_path}, starting from scratch")
        return 0, float('inf'), None, None, None

# EMA模型类
class EMAModel:
    def __init__(self, model, decay=0.9999):
        self.model = model
        self.decay = decay
        self.shadow = {}
        self.backup = {}
        
        # 初始化shadow
        for name, param in model.named_parameters():
            if param.requires_grad:
                self.shadow[name] = param.data.clone()
    
    def update(self):
        for name, param in self.model.named_parameters():
            if param.requires_grad:
                new_average = (1.0 - self.decay) * param.data + self.decay * self.shadow[name]
                self.shadow[name] = new_average.clone()
    
    def apply_shadow(self):
        for name, param in self.model.named_parameters():
            if param.requires_grad:
                self.backup[name] = param.data
                param.data = self.shadow[name]
    
    def restore(self):
        for name, param in self.model.named_parameters():
            if param.requires_grad:
                param.data = self.backup[name]
        self.backup = {}
    
    def state_dict(self):
        return self.shadow
    
    def load_state_dict(self, state_dict):
        self.shadow = state_dict
        self.backup = {}

# 全局变量用于保存训练状态
TRAINING_STATE = {
    'model': None,
    'optimizer': None,
    'scheduler': None,
    'epoch': 0,
    'best_loss': float('inf'),
    'scale': None,
    'device': None,
    'model_dir': None,
    'ema_model': None
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
            TRAINING_STATE['scheduler'],
            TRAINING_STATE['epoch'],
            TRAINING_STATE['best_loss'],
            TRAINING_STATE['scale'],
            TRAINING_STATE['device'],
            model_dir,
            filename='latent_flow_interrupted.pth'
        )
    sys.exit(0)

# 注册信号处理器
signal.signal(signal.SIGINT, signal_handler)

# ##############################
# 复用你的LatentUNet（仅改时间编码）
# ##############################
class SinusoidalPositionEmbeddings(nn.Module):
    def __init__(self, dim):
        super().__init__()
        self.dim = dim
    def forward(self, time):
        # 修复：使用较小的缩放因子，避免数值范围过大导致梯度爆炸
        # 原来的1000倍缩放对于CFM来说太大了
        time = time * 100.0  # 从1000降低到100
        device = time.device
        half_dim = self.dim // 2
        emb = math.log(10000) / (half_dim - 1)
        emb = torch.exp(torch.arange(half_dim, device=device) * -emb)
        emb = time[:, None] * emb[None, :]
        return torch.cat([emb.sin(), emb.cos()], dim=-1)

# ##############################
# 完全复用你的LatentBlock/UNet（残差+Concat+SiLU）
# ##############################
class LatentBlock(nn.Module):
    def __init__(self, in_ch, out_ch, time_emb_dim):
        super().__init__()
        self.time_mlp = nn.Linear(time_emb_dim, out_ch)
        self.conv1 = nn.Conv2d(in_ch, out_ch, 3, padding=1)
        self.conv2 = nn.Conv2d(out_ch, out_ch, 3, padding=1)
        # 动态GroupNorm，避坑固定维度
        num_groups = min(8, out_ch)
        while out_ch % num_groups !=0: num_groups -=1
        self.norm1 = nn.GroupNorm(num_groups, out_ch)
        self.norm2 = nn.GroupNorm(num_groups, out_ch)
        # 残差连接，避坑无残差导致梯度消失
        self.residual = nn.Conv2d(in_ch, out_ch, 1) if in_ch!=out_ch else nn.Identity()
        self.silu = nn.SiLU()  # 论文推荐，禁用ReLU

    def forward(self, x, t):
        res = self.residual(x)
        h = self.silu(self.norm1(self.conv1(x)))
        h += self.time_mlp(t)[...,None,None]
        h = self.silu(self.norm2(self.conv2(h)))
        return h + res  # 残差相加

class LatentUNet(nn.Module):
    def __init__(self, latent_channels=4, num_classes=2):
        super().__init__()
        time_dim = 32
        self.time_emb = nn.Sequential(SinusoidalPositionEmbeddings(time_dim), nn.Linear(time_dim, time_dim), nn.SiLU())
        self.class_emb = nn.Embedding(num_classes, time_dim)
        # 编码器
        self.down1 = LatentBlock(latent_channels, 64, time_dim)
        self.down2 = LatentBlock(64, 128, time_dim)
        # 中间层（不能使用Sequential，因为LatentBlock需要两个参数）
        self.mid1 = LatentBlock(128, 128, time_dim)
        self.mid2 = LatentBlock(128, 128, time_dim)
        # 解码器：Concat跳跃连接（避坑Add）
        self.up1 = LatentBlock(128+128, 64, time_dim)
        self.up2 = LatentBlock(64+64, latent_channels, time_dim)

    def forward(self, x, t, label=None):
        t = self.time_emb(t)
        if label is not None:
            t += self.class_emb(label)
        # 编码器+保存特征
        h1 = self.down1(x, t)
        h2 = self.down2(h1, t)
        # 中间层（逐个调用，不能使用Sequential）
        h = self.mid1(h2, t)
        h = self.mid2(h, t)
        # Concat跳跃连接（U-Net标准）
        h = self.up1(torch.cat([h, h2], dim=1), t)
        h = self.up2(torch.cat([h, h1], dim=1), t)
        return h  # 输出向量场v_t，维度和输入一致

# ##############################
# 训练主函数（替换DDPM为CFM）
# ##############################
def main():
    args = parse_args()
    device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
    os.makedirs(args.model_dir, exist_ok=True)
    
    logging.info(f"Model directory: {args.model_dir}")
    logging.info(f"Dataset: {args.dataset}")
    logging.info(f"Batch size: {args.batch_size}")
    logging.info(f"Epochs: {args.epochs}")
    logging.info(f"Latent channels: {args.latent_channels}")
    logging.info(f"Learning rate: {args.lr}")
    logging.info(f"Save interval: {args.save_interval}")

    # 1. 加载数据集（完全复用）
    loader = load_transformed_dataset(args.img_size, args.batch_size, args.dataset)
    dataset_size = len(loader.dataset)
    num_batches = len(loader)
    logging.info(f"Dataset size: {dataset_size} images")
    logging.info(f"Batches per epoch: {num_batches}")
    
    # 设备配置
    if torch.cuda.is_available():
        logging.info(f"CUDA available: {torch.cuda.is_available()}")
        logging.info(f"GPU name: {torch.cuda.get_device_name(0)}")
    logging.info(f"Using device: {device}")

    # 2. 加载冻结VAE（完全复用）
    vae = VAE(3, args.latent_channels, args.num_classes).to(device).eval()
    vae.load_state_dict(torch.load(args.vae_path, map_location=device, weights_only=True))
    for p in vae.parameters(): p.requires_grad = False
    logging.info(f"VAE loaded from {args.vae_path} (frozen)")
    
    # 3. 计算潜空间缩放（改进：使用更稳健的统计方法）
    logging.info("Computing latent space statistics...")
    scale = torch.tensor(1.0).to(device)
    with torch.no_grad():
        # 收集所有batch的统计信息
        all_stds = []
        all_means = []
        for b, l in loader:
            b, l = b.to(device), l.to(device)
            z = vae.encode_mu(b, l)
            all_stds.append(z.std().item())
            all_means.append(z.abs().mean().item())
        
        # 使用中位数而不是均值，更稳健
        std_median = torch.tensor(all_stds).median().item()
        mean_median = torch.tensor(all_means).median().item()
        
        # 综合考虑std和mean来计算scale
        scale = 1.0 / max(std_median, mean_median)
        
        logging.info(f'Latent std median: {std_median:.4f}, mean median: {mean_median:.4f}')
        logging.info(f'Latent scale: {scale:.4f}')

    # 4. 初始化CFM模型
    model = LatentUNet(args.latent_channels, args.num_classes).to(device)
    opt = Adam(model.parameters(), lr=args.lr)
    
    # 学习率调度器：参考train_ddpm_latent.py的配置
    sched = ReduceLROnPlateau(
        opt, 
        mode='min', 
        factor=0.5,  # 每次降低50%
        patience=50,  # 50个epoch没有改善就降低学习率
        min_lr=1e-6  # 最小学习率
    )
    logging.info(f"CFM parameters: {sum(p.numel() for p in model.parameters())}")
    logging.info(f"Learning rate scheduler: ReduceLROnPlateau (factor=0.5, patience=50)")
    
    # 初始化EMA模型
    ema_model = None
    if args.use_ema:
        ema_model = EMAModel(model, decay=args.ema_decay)
        logging.info(f"EMA enabled with decay={args.ema_decay}")
    
    # 尝试加载检查点（恢复训练）
    if args.resume:
        checkpoint_file = args.resume
        start_epoch, best_loss, loaded_scale, loaded_device, ema_state = load_checkpoint(
            model, opt, sched, args.model_dir, checkpoint_file, user_lr=args.lr
        )
        if loaded_scale is not None:
            scale = loaded_scale
        if ema_state is not None and ema_model is not None:
            ema_model.load_state_dict(ema_state)
            logging.info("EMA model state loaded from checkpoint")
    else:
        latest_checkpoint, checkpoint_type = find_latest_checkpoint(args.model_dir)
        if latest_checkpoint:
            logging.info(f"Found latest checkpoint: {os.path.basename(latest_checkpoint)} (type: {checkpoint_type})")
            start_epoch, best_loss, loaded_scale, loaded_device, ema_state = load_checkpoint(
                model, opt, sched, args.model_dir, os.path.basename(latest_checkpoint), user_lr=args.lr
            )
            if loaded_scale is not None:
                scale = loaded_scale
            if ema_state is not None and ema_model is not None:
                ema_model.load_state_dict(ema_state)
                logging.info("EMA model state loaded from checkpoint")
        else:
            logging.info("No checkpoint found, starting from scratch")
            start_epoch, best_loss = 0, float('inf')
    
    # 更新全局训练状态
    TRAINING_STATE['model'] = model
    TRAINING_STATE['optimizer'] = opt
    TRAINING_STATE['scheduler'] = sched
    TRAINING_STATE['epoch'] = start_epoch
    TRAINING_STATE['best_loss'] = best_loss
    TRAINING_STATE['scale'] = scale
    TRAINING_STATE['device'] = device
    TRAINING_STATE['model_dir'] = args.model_dir
    TRAINING_STATE['ema_model'] = ema_model
    
    # 学习率预热函数
    def get_warmup_lr(epoch, warmup_epochs, base_lr):
        if epoch < warmup_epochs:
            return base_lr * (epoch + 1) / warmup_epochs
        return base_lr
    
    # 更新全局训练状态
    TRAINING_STATE['model'] = model
    TRAINING_STATE['optimizer'] = opt
    TRAINING_STATE['scheduler'] = sched
    TRAINING_STATE['epoch'] = start_epoch
    TRAINING_STATE['best_loss'] = best_loss
    TRAINING_STATE['scale'] = scale
    TRAINING_STATE['device'] = device
    TRAINING_STATE['model_dir'] = args.model_dir

    # 训练循环
    for epoch in range(start_epoch, args.epochs):
        # 学习率预热
        if epoch < args.warmup_epochs:
            warmup_lr = get_warmup_lr(epoch, args.warmup_epochs, args.lr)
            for param_group in opt.param_groups:
                param_group['lr'] = warmup_lr
        
        model.train()
        total_loss = 0.0
        for img, label in tqdm(loader, desc=f'Epoch {epoch}/{args.epochs}'):
            img, label = img.to(device), label.to(device)
            opt.zero_grad()
            # 步骤1：VAE编码潜变量
            with torch.no_grad():
                x1 = vae.encode_mu(img, label) * scale  # 缩放潜空间
            # 步骤2：采样噪声x0~N(0,I)
            x0 = torch.randn_like(x1)
            # 步骤3：采样连续时间t
            t = sample_continuous_t(x1.shape[0], device)
            # 步骤4：OT路径计算psi_t, u_t（对标论文）
            psi_t, u_t = ot_path(x0, x1, t)
            # 步骤5：模型预测向量场v_t
            v_t = model(psi_t, t, label)
            # 步骤6：CFM损失
            loss = cfm_loss(v_t, u_t)
            # 反向传播
            loss.backward()
            # 使用更小的梯度裁剪阈值
            torch.nn.utils.clip_grad_norm_(model.parameters(), args.grad_clip)
            opt.step()
            
            # 更新EMA模型
            if ema_model is not None:
                ema_model.update()
            
            total_loss += loss.item()
        # 日志/保存
        avg_loss = total_loss / len(loader)
        sched.step(avg_loss)
        
        # 更新全局训练状态
        TRAINING_STATE['epoch'] = epoch
        TRAINING_STATE['best_loss'] = best_loss
        
        # 保存最佳模型（优先使用EMA模型）
        if avg_loss < best_loss:
            best_loss = avg_loss
            if ema_model is not None:
                ema_model.apply_shadow()
                safe_save_model(model.state_dict(), os.path.join(args.model_dir, 'best_flow.pth'))
                ema_model.restore()
                logging.info(f"✓ New best model (EMA) saved with loss: {best_loss:.4f}")
            else:
                safe_save_model(model.state_dict(), os.path.join(args.model_dir, 'best_flow.pth'))
                logging.info(f"✓ New best model saved with loss: {best_loss:.4f}")
        
        # 定期保存检查点
        if (epoch + 1) % args.save_interval == 0:
            save_checkpoint(
                model, opt, sched, epoch + 1, best_loss, scale, device,
                args.model_dir, filename=f'latent_flow_epoch_{epoch + 1}.pth', ema_model=ema_model
            )
            logging.info(f"✓ Checkpoint saved at epoch {epoch + 1}")
        
        # 记录当前学习率
        current_lr = opt.param_groups[0]['lr']
        logging.info(f'Epoch {epoch}/{args.epochs} | Loss: {avg_loss:.4f} | Best: {best_loss:.4f} | LR: {current_lr:.6f}')
    
    # 训练完成，保存最终模型
    if ema_model is not None:
        ema_model.apply_shadow()
        save_checkpoint(
            model, opt, sched, args.epochs, best_loss, scale, device,
            args.model_dir, filename='latent_flow_final.pth', ema_model=ema_model
        )
        ema_model.restore()
        logging.info(f"Training completed! Best loss: {best_loss:.4f} (with EMA)")
    else:
        save_checkpoint(
            model, opt, sched, args.epochs, best_loss, scale, device,
            args.model_dir, filename='latent_flow_final.pth'
        )
        logging.info(f"Training completed! Best loss: {best_loss:.4f}")

if __name__ == '__main__':
    signal.signal(signal.SIGINT, signal.default_int_handler)
    main()