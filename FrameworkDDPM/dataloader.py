from torchvision import transforms
from torch.utils.data import DataLoader
import numpy as np
import torch
import torchvision
import matplotlib.pyplot as plt


def load_transformed_dataset(img_size=256, batch_size=128, dataset_name="datasets-1", shuffle=True) -> DataLoader:
    # Load dataset and perform data transformations
    data_transforms = [
        transforms.Resize((img_size, img_size)),
        transforms.ToTensor(),  # Scales data into [0,1]
        transforms.Lambda(lambda t: (t * 2) - 1),  # Scale between [-1, 1]
    ]
    data_transform = transforms.Compose(data_transforms)

    # 根据数据集名称选择不同的数据集
    train = torchvision.datasets.ImageFolder(root=f"./{dataset_name}/train", transform=data_transform)

    # 使用train文件夹中的所有图片进行训练
    dataset = train

    # 对于datasets-2，我们需要返回类别编码
    # ImageFolder会自动返回 (image, label) 对，其中label是类别索引
    return DataLoader(dataset, batch_size=batch_size, shuffle=shuffle, drop_last=False)


def show_tensor_image(image):
    # Reverse the data transformations
    reverse_transforms = transforms.Compose(
        [
            transforms.Lambda(lambda t: (t + 1) / 2),
            transforms.Lambda(lambda t: t.permute(1, 2, 0)),  # CHW to HWC
            transforms.Lambda(lambda t: t * 255.0),
            transforms.Lambda(lambda t: t.cpu().numpy().astype(np.uint8)),  # Move to CPU before converting to numpy
            transforms.ToPILImage(),
        ]
    )

    # Take first image of batch
    if len(image.shape) == 4:
        image = image[0, :, :, :]
    plt.imshow(reverse_transforms(image))