import torch
import numpy as np
import matplotlib.pyplot as plt
from umap import UMAP
from vae import VAE
from dataloader import load_transformed_dataset

# 加载VAE
device = "cuda" if torch.cuda.is_available() else "cpu"
vae = VAE(in_channels=3, latent_channels=4, num_classes=2).to(device)
vae.load_state_dict(torch.load(r"F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model\best_vae.pth", map_location=device, weights_only=True))
vae.eval()

# 加载数据
dataloader = load_transformed_dataset(img_size=64, batch_size=8, dataset_name="datasets-3")

# 提取所有样本的潜向量和标签
all_z = []
all_labels = []
with torch.no_grad():
    for batch, labels in dataloader:
        batch = batch.to(device)
        labels = labels.to(device)
        z = vae.encode(batch, labels)
        # 将 4D 特征图 (batch, channels, height, width) 展平为 2D (batch, channels*height*width)
        z_flat = z.reshape(z.shape[0], -1)
        all_z.append(z_flat.cpu().numpy())
        all_labels.append(labels.cpu().numpy())

all_z = np.concatenate(all_z, axis=0)
all_labels = np.concatenate(all_labels, axis=0)

# UMAP降维到2D
umap = UMAP(n_components=2, random_state=42)
z_2d = umap.fit_transform(all_z)

# 可视化
plt.figure(figsize=(8, 8))
scatter = plt.scatter(z_2d[:, 0], z_2d[:, 1], c=all_labels, cmap='viridis', alpha=0.7)
plt.legend(handles=scatter.legend_elements()[0], labels=['Class 0', 'Class 1'])
plt.title('VAE Latent Space UMAP Visualization')
plt.savefig(r"F:\CG2026\homework in winter\USTC_CG_26\FrameworkDDPM\model\vae_latent_umap.png")
plt.close()
print("潜空间可视化已保存到 vae_latent_umap.png")