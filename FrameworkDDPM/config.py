import argparse
import os

class Config:
    def __init__(self):
        self.parser = argparse.ArgumentParser(description='DDPM Training and Sampling')
        
        # 数据集相关参数
        self.parser.add_argument('--dataset', type=str, default='datasets-1',
                                choices=['datasets-1', 'datasets-2'],
                                help='选择训练数据集 (datasets-1 或 datasets-2)')
        
        # 模型保存相关参数
        self.parser.add_argument('--model_dir', type=str,
                                default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model',
                                help='模型保存目录路径')
        self.parser.add_argument('--model_name', type=str, default='best_model.pth',
                                help='模型文件名')
        self.parser.add_argument('--save_interval', type=int, default=500,
                                help='每多少个epoch保存一次检查点')
        
        # 训练相关参数
        self.parser.add_argument('--batch_size', type=int, default=1,
                                help='批次大小')
        self.parser.add_argument('--epochs', type=int, default=5000,
                                help='训练轮数')
        self.parser.add_argument('--img_size', type=int, default=64,
                                help='输入图像大小')
        self.parser.add_argument('--T', type=int, default=300,
                                help='扩散时间步数')
        self.parser.add_argument('--lr', type=float, default=1e-4,
                                help='学习率')
        
        # 其他参数
        self.parser.add_argument('--resume', type=str, default=None,
                                help='从指定检查点恢复训练 (checkpoint文件名)')
        self.parser.add_argument('--log_interval', type=int, default=50,
                                help='每多少个batch输出一次日志')
        
        # 输出目录
        self.parser.add_argument('--output_dir', type=str,
                                default=r'F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\output',
                                help='输出目录路径')
        
        self.args = None
    
    def parse_args(self):
        if self.args is None:
            self.args = self.parser.parse_args()
        return self.args
    
    def get_args(self):
        return self.parse_args()

# 全局配置实例
config = Config()