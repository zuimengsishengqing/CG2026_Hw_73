
from torch import nn
import torch
import math
import logging


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


class Encoder(nn.Module):
    def __init__(self, in_channels=3, latent_channels=4, num_classes=2):
        super().__init__()
        self.latent_channels = latent_channels
        
        # 类别嵌入层
        self.class_emb = nn.Embedding(num_classes, 64)
        
        # 编码器：将图像压缩为潜在特征图
        # 输入: (batch, 3, 64, 64)
        self.conv1 = nn.Conv2d(in_channels, 64, 4, 2, 1)  # (batch, 64, 32, 32)
        self.conv2 = nn.Conv2d(64, 128, 4, 2, 1)  # (batch, 128, 16, 16)
        self.conv3 = nn.Conv2d(128, 256, 4, 2, 1)  # (batch, 256, 8, 8)
        self.conv4 = nn.Conv2d(256, 512, 4, 2, 1)  # (batch, 512, 4, 4)
        
        # 输出潜在特征图的均值和对数方差
        self.conv_mu = nn.Conv2d(512, latent_channels, 1)  # (batch, latent_channels, 4, 4)
        self.conv_logvar = nn.Conv2d(512, latent_channels, 1)  # (batch, latent_channels, 4, 4)
        
        # 类别条件注入：将类别嵌入广播到特征图空间
        self.fc_class = nn.Linear(64, 512)
        
        self.gn1 = nn.GroupNorm(8, 64)
        self.gn2 = nn.GroupNorm(8, 128)
        self.gn3 = nn.GroupNorm(8, 256)
        self.gn4 = nn.GroupNorm(8, 512)
        
        self.relu = nn.ReLU()
    
    def forward(self, x, class_label=None):
        # 卷积编码
        h = self.relu(self.gn1(self.conv1(x)))
        h = self.relu(self.gn2(self.conv2(h)))
        h = self.relu(self.gn3(self.conv3(h)))
        h = self.relu(self.gn4(self.conv4(h)))
        
        # 融合类别条件
        if class_label is not None:
            c_emb = self.class_emb(class_label)
            c_emb = self.relu(self.fc_class(c_emb))
            # 将类别嵌入广播到特征图空间
            c_emb = c_emb.view(-1, 512, 1, 1)
            h = h + c_emb
        
        # 输出均值和对数方差（特征图形式）
        mu = self.conv_mu(h)
        logvar = self.conv_logvar(h)
        
        return mu, logvar


class Decoder(nn.Module):
    def __init__(self, out_channels=3, latent_channels=4, num_classes=2):
        super().__init__()
        self.latent_channels = latent_channels
        
        # 类别嵌入层
        self.class_emb = nn.Embedding(num_classes, 64)
        
        # 从潜在特征图到卷积特征
        self.conv_in = nn.Conv2d(latent_channels, 512, 1)  # (batch, 512, 4, 4)
        
        # 类别条件注入
        self.fc_class = nn.Linear(64, 512)
        
        # 解码器：从潜在特征图还原图像
        self.deconv1 = nn.ConvTranspose2d(512, 256, 4, 2, 1)  # (batch, 256, 8, 8)
        self.deconv2 = nn.ConvTranspose2d(256, 128, 4, 2, 1)  # (batch, 128, 16, 16)
        self.deconv3 = nn.ConvTranspose2d(128, 64, 4, 2, 1)  # (batch, 64, 32, 32)
        self.deconv4 = nn.ConvTranspose2d(64, out_channels, 4, 2, 1)  # (batch, 3, 64, 64)
        
        self.gn1 = nn.GroupNorm(8, 256)
        self.gn2 = nn.GroupNorm(8, 128)
        self.gn3 = nn.GroupNorm(8, 64)
        
        self.relu = nn.ReLU()
        self.tanh = nn.Tanh()  # 输出范围[-1, 1]
    
    def forward(self, z, class_label=None):
        # 从潜在特征图到卷积特征
        h = self.relu(self.conv_in(z))
        
        # 融合类别条件
        if class_label is not None:
            c_emb = self.class_emb(class_label)
            c_emb = self.relu(self.fc_class(c_emb))
            # 将类别嵌入广播到特征图空间
            c_emb = c_emb.view(-1, 512, 1, 1)
            h = h + c_emb
        
        # 解卷积解码
        h = self.relu(self.gn1(self.deconv1(h)))
        h = self.relu(self.gn2(self.deconv2(h)))
        h = self.relu(self.gn3(self.deconv3(h)))
        x_recon = self.tanh(self.deconv4(h))
        
        return x_recon


class VAE(nn.Module):
    def __init__(self, in_channels=3, latent_channels=4, num_classes=2):
        super().__init__()
        self.encoder = Encoder(in_channels, latent_channels, num_classes)
        self.decoder = Decoder(in_channels, latent_channels, num_classes)
        
        # 潜在空间缩放因子（类似Stable Diffusion）
        # 确保输入DDPM的潜在向量方差接近1
        # 使用nn.Parameter使其可以被state_dict()保存和加载
        self.scaling_factor = nn.Parameter(torch.tensor(1.0))
    
    def reparameterize(self, mu, logvar):
        """
        重参数化采样：z = μ + ε * σ
        其中 ε ~ N(0, I)，σ = exp(logvar / 2)
        """
        std = torch.exp(0.5 * logvar)
        eps = torch.randn_like(std)
        return mu + eps * std
    
    def forward(self, x, class_label=None):
        # 编码
        mu, logvar = self.encoder(x, class_label)
        
        # 重参数化采样
        z = self.reparameterize(mu, logvar)
        
        # 解码
        x_recon = self.decoder(z, class_label)
        
        return x_recon, mu, logvar, z
    
    def encode(self, x, class_label=None):
        """仅编码，用于DDPM训练（使用重参数化采样）"""
        mu, logvar = self.encoder(x, class_label)
        z = self.reparameterize(mu, logvar)
        # 应用缩放因子，确保潜在特征图方差接近1
        # scaling_factor是nn.Parameter，需要使用.item()获取数值
        z = z * self.scaling_factor.item()
        return z
    
    def encode_mu(self, x, class_label=None):
        """编码并直接返回均值 mu（不进行重参数化采样），用于DDPM训练
        
        这种方法可以提高训练稳定性，因为目标分布更加确定，
        不会因为重参数化采样的随机性而增加学习难度。
        """
        mu, logvar = self.encoder(x, class_label)
        # 应用缩放因子
        mu = mu * self.scaling_factor.item()
        return mu
    
    def encode_unscaled(self, x, class_label=None):
        """编码但不缩放，用于计算缩放因子"""
        mu, logvar = self.encoder(x, class_label)
        z = self.reparameterize(mu, logvar)
        return z
    
    def decode(self, z, class_label=None):
        """仅解码，用于DDPM采样"""
        # 反向缩放，因为DDPM采样是在缩放后的空间进行的
        # scaling_factor是nn.Parameter，需要使用.item()获取数值
        z_unscaled = z / self.scaling_factor.item()
        return self.decoder(z_unscaled, class_label)
    
    def compute_scaling_factor(self, dataloader, device, num_samples=1000):
        """
        计算潜在空间的标准差，确定缩放因子
        
        Args:
            dataloader: 数据加载器
            device: 设备
            num_samples: 采样数量
        
        Returns:
            scaling_factor: 缩放因子
        """
        self.eval()
        all_z = []
        
        with torch.no_grad():
            count = 0
            for batch, class_labels in dataloader:
                if count >= num_samples:
                    break
                
                batch = batch.to(device)
                class_labels = class_labels.to(device)
                
                # 编码但不缩放
                z = self.encode_unscaled(batch, class_labels)
                all_z.append(z.flatten().cpu())
                
                count += batch.shape[0]
        
        # 计算所有潜在向量的标准差
        all_z = torch.cat(all_z, dim=0)
        std = torch.std(all_z).item()
        
        # 缩放因子 = 1 / std，确保缩放后的标准差接近1
        scaling_factor = 1.0 / (std + 1e-8)
        
        logging.info(f"潜在空间标准差: {std:.4f}")
        logging.info(f"计算得到的缩放因子: {scaling_factor:.4f}")
        
        # 更新VAE的scaling_factor（nn.Parameter）
        self.scaling_factor.data.fill_(scaling_factor)
        
        return scaling_factor


def vae_loss_function(x_recon, x, mu, logvar, beta=0.0001, use_l1_loss=False):
    """
    VAE损失函数 = 重建损失 + beta * KL散度损失
    
    Args:
        x_recon: 重建的图像
        x: 原始图像
        mu: 编码器输出的均值
        logvar: 编码器输出的对数方差
        beta: KL散度的权重系数（默认0.0001，平衡重建和KL）
        use_l1_loss: 是否使用 L1 损失（绝对误差）替代 MSE
                   L1 损失比 MSE 更能保留图像的高频细节和锐利边缘
    
    Returns:
        total_loss: 总损失（每个样本的平均损失）
        recon_loss: 重建损失（每个样本的平均损失）
        kl_loss: KL散度损失（每个样本的平均损失）
    """
    # 重建损失 - 使用sum而不是mean
    # 这样重建损失和KL散度在同一个数量级，beta才能发挥作用
    if use_l1_loss:
        # L1 损失（绝对误差）：更能保留高频细节和锐利边缘
        recon_loss = nn.functional.l1_loss(x_recon, x, reduction='sum')
    else:
        # MSE 损失（均方误差）：容易导致输出模糊的灰色块
        recon_loss = nn.functional.mse_loss(x_recon, x, reduction='sum')
    
    # KL散度损失：KL(N(μ, σ²) || N(0, I))
    # 公式：-0.5 * sum(1 + log(σ²) - μ² - σ²)
    # 使用sum而不是mean，与重建损失保持一致
    kl_loss = -0.5 * torch.sum(1 + logvar - mu.pow(2) - logvar.exp())
    
    # 总损失
    total_loss = recon_loss + beta * kl_loss
    
    return total_loss, recon_loss, kl_loss


if __name__ == "__main__":
    # 测试VAE
    vae = VAE(in_channels=3, latent_channels=4, num_classes=2)
    print(f"VAE参数数量: {sum(p.numel() for p in vae.parameters())}")
    
    # 测试前向传播
    batch_size = 4
    x = torch.randn(batch_size, 3, 64, 64)
    class_label = torch.randint(0, 2, (batch_size,))
    
    x_recon, mu, logvar, z = vae(x, class_label)
    print(f"输入形状: {x.shape}")
    print(f"重建形状: {x_recon.shape}")
    print(f"均值形状: {mu.shape}")
    print(f"对数方差形状: {logvar.shape}")
    print(f"潜在特征图形状: {z.shape}")
    
    # 测试损失函数
    loss, recon_loss, kl_loss = vae_loss_function(x_recon, x, mu, logvar)
    print(f"总损失: {loss.item():.4f}")
    print(f"重建损失: {recon_loss.item():.4f}")
    print(f"KL散度损失: {kl_loss.item():.4f}")