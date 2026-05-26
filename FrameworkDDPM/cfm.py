import torch
import torch.nn.functional as F

# 论文默认参数，严格对标Example II
SIGMA_MIN = 0.001  # 公式20/22，保证t=0为标准高斯

def sample_continuous_t(batch_size: int, device: torch.device) -> torch.Tensor:
    """采样连续时间 t ~ Uniform[0,1]，对标论文3.2节"""
    return torch.rand(batch_size, device=device)

def ot_path(x0: torch.Tensor, x1: torch.Tensor, t: torch.Tensor) -> tuple[torch.Tensor, torch.Tensor]:
    """
    严格对标论文Example II，修复数值爆炸问题
    输入：x0~N(0,I)噪声, x1=VAE潜变量, t∈[0,1]
    返回：psi_t(流状态), u_t(目标向量场)
    公式22 + 公式21
    
    修复：当t→1时，分母接近SIGMA_MIN，导致u_t数值爆炸
    解决方案：对u_t进行数值裁剪，防止极端值
    """
    # 维度广播：[B,] → [B,1,1,1]
    t = t.view(-1, 1, 1, 1)
    # 公式22：OT流映射
    psi_t = (1 - (1 - SIGMA_MIN) * t) * x0 + t * x1
    # 公式21：目标向量场，使用更大的防除零常数
    denom = 1 - (1 - SIGMA_MIN) * t + 1e-6
    u_t = (x1 - (1 - SIGMA_MIN) * x0) / denom
    
    # 数值裁剪：防止极端值导致训练不稳定
    # 根据经验，u_t的合理范围应该在[-10, 10]之间
    u_t = torch.clamp(u_t, min=-10.0, max=10.0)
    
    return psi_t, u_t

def cfm_loss(pred_v: torch.Tensor, target_u: torch.Tensor) -> torch.Tensor:
    """CFM损失：MSE拟合向量场，对标公式9"""
    return F.mse_loss(pred_v, target_u)