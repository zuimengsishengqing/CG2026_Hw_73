from forward_noising import forward_diffusion_sample
from unet import SimpleUnet
from dataloader import load_transformed_dataset
import torch.nn.functional as F
import torch
from torch.optim import Adam
import logging
from tqdm import trange
import cv2 as cv
import os
import signal
import sys
import argparse

logging.basicConfig(level=logging.INFO)

def parse_args():
    """解析命令行参数"""
    parser = argparse.ArgumentParser(description='DDPM Training Script')
    
    # 数据集相关参数
    parser.add_argument('--dataset', type=str, default='datasets-1',
                        choices=['datasets-1', 'datasets-2'],
                        help='选择训练数据集 (datasets-1 或 datasets-2)')
    
    # 模型保存相关参数
    parser.add_argument('--model_dir', type=str,
                        default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                        help='模型保存目录路径')
    parser.add_argument('--save_interval', type=int, default=1000,
                        help='每多少个epoch保存一次检查点')
    
    # 训练相关参数
    parser.add_argument('--batch_size', type=int, default=1,
                        help='批次大小')
    parser.add_argument('--epochs', type=int, default=5000,
                        help='训练轮数')
    parser.add_argument('--img_size', type=int, default=64,
                        help='输入图像大小')
    parser.add_argument('--T', type=int, default=300,
                        help='扩散时间步数')
    parser.add_argument('--lr', type=float, default=1e-4,
                        help='学习率')
    
    # 其他参数
    parser.add_argument('--resume', type=str, default=None,
                        help='从指定检查点恢复训练 (checkpoint文件名)')
    parser.add_argument('--log_interval', type=int, default=50,
                        help='每多少个batch输出一次日志')
    
    return parser.parse_args()

# 解析命令行参数
args = parse_args()

# 模型存储目录配置
MODEL_DIR = args.model_dir
os.makedirs(MODEL_DIR, exist_ok=True) 
logging.info(f"Model directory: {MODEL_DIR}")
logging.info(f"Dataset: {args.dataset}")
logging.info(f"Batch size: {args.batch_size}")
logging.info(f"Epochs: {args.epochs}")
logging.info(f"Image size: {args.img_size}")
logging.info(f"Time steps (T): {args.T}")
logging.info(f"Save interval: {args.save_interval}")

# 全局变量用于保存训练状态
TRAINING_STATE = {
    'model': None,
    'optimizer': None,
    'epoch': 0,
    'batch_idx': 0,
    'best_loss': float('inf'),
    'device': None
}

def save_checkpoint(model, optimizer, epoch, batch_idx, best_loss, device, filename='checkpoint.pth', max_retries=3):
    """
    保存训练检查点，包括模型、优化器状态和训练进度
    使用临时文件+原子重命名的方式避免文件访问冲突
    添加重试机制处理磁盘IO错误
    """
    checkpoint = {
        'model_state_dict': model.state_dict(),
        'optimizer_state_dict': optimizer.state_dict(),
        'epoch': epoch,
        'batch_idx': batch_idx,
        'best_loss': best_loss,
        'device': device
    }
    
    # 使用模型目录路径
    checkpoint_path = os.path.join(MODEL_DIR, filename)
    temp_filename = f"{checkpoint_path}.tmp"
    
    for attempt in range(max_retries):
        try:
            # 清理可能存在的临时文件
            if os.path.exists(temp_filename):
                os.remove(temp_filename)
            
            # 先保存到临时文件
            torch.save(checkpoint, temp_filename)
            
            # 原子重命名（Windows下会自动替换目标文件）
            if os.path.exists(checkpoint_path):
                os.remove(checkpoint_path)
            os.rename(temp_filename, checkpoint_path)
            
            logging.info(f"Checkpoint saved to {checkpoint_path} at epoch {epoch}, batch {batch_idx}")
            return True
            
        except Exception as e:
            logging.warning(f"Attempt {attempt + 1}/{max_retries} failed to save checkpoint: {e}")
            if os.path.exists(temp_filename):
                try:
                    os.remove(temp_filename)
                except:
                    pass
            
            if attempt < max_retries - 1:
                import time
                time.sleep(1)  # 等待1秒后重试
            else:
                logging.error(f"Failed to save checkpoint after {max_retries} attempts: {e}")
                return False

def load_checkpoint(model, optimizer, filename='checkpoint.pth'):
    """
    加载训练检查点，恢复训练状态
    """
    checkpoint_path = os.path.join(MODEL_DIR, filename)
    if os.path.exists(checkpoint_path):
        # 注意：checkpoint包含多个状态字典，不能使用weights_only=True
        checkpoint = torch.load(checkpoint_path, weights_only=False)
        model.load_state_dict(checkpoint['model_state_dict'])
        optimizer.load_state_dict(checkpoint['optimizer_state_dict'])
        epoch = checkpoint['epoch']
        batch_idx = checkpoint['batch_idx']
        best_loss = checkpoint['best_loss']
        device = checkpoint['device']
        logging.info(f"Checkpoint loaded from {checkpoint_path}: epoch {epoch}, batch {batch_idx}")
        return epoch, batch_idx, best_loss, device
    else:
        logging.info(f"No checkpoint found at {checkpoint_path}, starting from scratch")
        return 0, 0, float('inf'), None

def signal_handler(sig, frame):
    """
    处理Ctrl+C信号，保存训练状态后退出
    """
    logging.info("\nTraining interrupted by user. Saving checkpoint...")
    if TRAINING_STATE['model'] is not None and TRAINING_STATE['optimizer'] is not None:
        save_checkpoint(
            TRAINING_STATE['model'],
            TRAINING_STATE['optimizer'],
            TRAINING_STATE['epoch'],
            TRAINING_STATE['batch_idx'],
            TRAINING_STATE['best_loss'],
            TRAINING_STATE['device'],
            filename=os.path.join(MODEL_DIR, 'interrupted_checkpoint.pth')
        )
    sys.exit(0)

# 注册信号处理器
signal.signal(signal.SIGINT, signal_handler)

# TODO: 完成训练过程的Loss计算
# 加噪过程需要补充forward_diffusion_sample中内容，并调用
def get_loss(model, x_0, t, device, class_label=None):
    # 前向加噪：根据时间步t对原始图像x_0加噪，得到x_t
    x_noisy, noise = forward_diffusion_sample(x_0, t, device)
    
    # 模型预测：用UNet模型预测噪声，输入是加噪图像x_t、时间步t和类别标签class_label
    predicted_noise = model(x_noisy, t, class_label)
    
    # 计算损失：真实噪声与预测噪声之间的均方误差（MSE）
    loss = F.mse_loss(noise, predicted_noise)
    
    return loss


if __name__ == "__main__":
    # 从命令行参数获取训练配置
    T = args.T
    BATCH_SIZE = args.batch_size
    epochs = args.epochs
    save_interval = args.save_interval
    img_size = args.img_size
    dataset_name = args.dataset
    lr = args.lr
    log_interval = args.log_interval
    
    # 数据加载
    dataloader = load_transformed_dataset(img_size=img_size, batch_size=BATCH_SIZE, dataset_name=dataset_name)
    
    # 打印数据集信息
    dataset_size = len(dataloader.dataset)
    num_batches = len(dataloader)
    logging.info(f"Dataset size: {dataset_size} images")
    logging.info(f"Batch size: {BATCH_SIZE}")
    logging.info(f"Batches per epoch: {num_batches}")
    
    if num_batches == 0:
        logging.error("ERROR: No batches to train! Dataset is too small for the current batch size.")
        logging.error(f"Please reduce BATCH_SIZE from {BATCH_SIZE} to at most {dataset_size}")
        logging.error("Or check if your dataset path is correct.")
        sys.exit(1)
    
    # 设备配置
    device = "cuda" if torch.cuda.is_available() else "cpu"
    if torch.cuda.is_available():
        logging.info(f"CUDA available: {torch.cuda.is_available()}")
        logging.info(f"CUDA version: {torch.version.cuda}")
        logging.info(f"GPU count: {torch.cuda.device_count()}")
        logging.info(f"GPU name: {torch.cuda.get_device_name(0)}")
        logging.info(f"Current device: {torch.cuda.current_device()}")
    logging.info(f"Using device: {device}")
    
    # 模型和优化器初始化
    model = SimpleUnet()
    model.to(device)
    optimizer = Adam(model.parameters(), lr=lr)
    
    # 尝试加载检查点（恢复训练）
    checkpoint_file = args.resume if args.resume else 'checkpoint.pth'
    start_epoch, start_batch_idx, best_loss, loaded_device = load_checkpoint(model, optimizer, checkpoint_file)
    
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
    
    # 训练循环
    logging.info(f"Starting training from epoch {start_epoch}, batch {start_batch_idx}")
    
    for epoch in range(start_epoch, epochs):
        TRAINING_STATE['epoch'] = epoch
        
        # 如果是恢复训练，从上次中断的batch开始
        batch_start = start_batch_idx if epoch == start_epoch else 0
        
        for batch_idx, (batch, class_labels) in enumerate(dataloader):
            TRAINING_STATE['batch_idx'] = batch_idx
            
            # 获取批次数据（已通过enumerate正确获取）
            
            # 清零梯度：清除之前的梯度信息
            optimizer.zero_grad()

            # 数据移动到设备：将批次数据移动到GPU或CPU
            batch = batch.to(device)
            
            # 类别标签移动到设备（对于datasets-2，class_labels是类别索引；对于datasets-1，class_labels为None）
            if class_labels is not None:
                class_labels = class_labels.to(device)

            # 时间步采样：从[0, T)范围内随机采样时间步
            t = torch.randint(0, T, (batch.shape[0],), device=device).long()

            # 计算损失：调用get_loss函数计算当前批次的损失，传入类别标签
            loss = get_loss(model, batch, t, device, class_label=class_labels)

            # 反向传播：计算梯度
            loss.backward()

            # 参数更新：使用优化器更新模型参数
            optimizer.step()

            # 更新最佳损失
            if loss.item() < best_loss:
                best_loss = loss.item()
                TRAINING_STATE['best_loss'] = best_loss
                # 保存最佳模型（使用临时文件避免冲突，添加重试机制）
                best_model_path = os.path.join(MODEL_DIR, 'best_model.pth')
                temp_filename = f"{best_model_path}.tmp"
                max_retries = 3
                for attempt in range(max_retries):
                    try:
                        if os.path.exists(temp_filename):
                            os.remove(temp_filename)
                        torch.save(model.state_dict(), temp_filename)
                        if os.path.exists(best_model_path):
                            os.remove(best_model_path)
                        os.rename(temp_filename, best_model_path)
                        logging.info(f"New best model saved with loss: {best_loss:.6f}")
                        break
                    except Exception as e:
                        logging.warning(f"Attempt {attempt + 1}/{max_retries} failed to save best model: {e}")
                        if os.path.exists(temp_filename):
                            try:
                                os.remove(temp_filename)
                            except:
                                pass
                        if attempt < max_retries - 1:
                            import time
                            time.sleep(1)
                        else:
                            logging.error(f"Failed to save best model after {max_retries} attempts: {e}")

            # 日志输出：根据命令行参数间隔输出训练信息
            if batch_idx % log_interval == 0:
                logging.info(f"Epoch {epoch} | Batch index {batch_idx:03d} Loss: {loss.item():.6f} | Best Loss: {best_loss:.6f}")
        
        # 每个epoch结束时的日志
        logging.info(f"Epoch {epoch} completed")
        
        # 定期保存检查点
        if (epoch + 1) % save_interval == 0:
            save_checkpoint(
                model, optimizer, epoch + 1, 0, best_loss, device,
                filename=os.path.join(MODEL_DIR, f'checkpoint_epoch_{epoch + 1}.pth')
            )
            # 同时更新主检查点
            save_checkpoint(
                model, optimizer, epoch + 1, 0, best_loss, device,
                filename=os.path.join(MODEL_DIR, 'checkpoint.pth')
            )
    
    # 训练完成，保存最终模型
    logging.info("Training completed!")
    final_model_path = os.path.join(MODEL_DIR, f"ddpm_mse_epochs_{epochs}.pth")
    temp_filename = f"{final_model_path}.tmp"
    max_retries = 3
    for attempt in range(max_retries):
        try:
            if os.path.exists(temp_filename):
                os.remove(temp_filename)
            torch.save(model.state_dict(), temp_filename)
            if os.path.exists(final_model_path):
                os.remove(final_model_path)
            os.rename(temp_filename, final_model_path)
            logging.info(f"Final model saved to {final_model_path}")
            break
        except Exception as e:
            logging.warning(f"Attempt {attempt + 1}/{max_retries} failed to save final model: {e}")
            if os.path.exists(temp_filename):
                try:
                    os.remove(temp_filename)
                except:
                    pass
            if attempt < max_retries - 1:
                import time
                time.sleep(1)
            else:
                logging.error(f"Failed to save final model after {max_retries} attempts: {e}")
    
    # 保存最终检查点
    save_checkpoint(
        model, optimizer, epochs, 0, best_loss, device,
        filename=os.path.join(MODEL_DIR, 'final_checkpoint.pth')
    )
    logging.info(f"Training finished. Best loss: {best_loss:.6f}")