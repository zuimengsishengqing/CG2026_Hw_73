
import torch
import sys
import os

print("=" * 80)
print("CUDA GPU 详细诊断")
print("=" * 80)

print("\n【系统信息】")
print(f"Python版本: {sys.version}")
print(f"操作系统: {os.name}")
print(f"工作目录: {os.getcwd()}")

print("\n【PyTorch信息】")
print(f"PyTorch版本: {torch.__version__}")
print(f"PyTorch编译版本: {torch.version.git_version if hasattr(torch.version, 'git_version') else 'N/A'}")
print(f"PyTorch CUDA版本: {torch.version.cuda}")
print(f"CUDA可用: {torch.cuda.is_available()}")

print("\n【CUDA详细信息】")
cuda_available = torch.cuda.is_available()
print(f"1. CUDA 可用性: {cuda_available}")

if not cuda_available:
    print("\n【问题诊断】")
    print("CUDA不可用的可能原因：")
    
    # 检查PyTorch是否是CPU版本
    print(f"\n2. PyTorch是否为CPU版本: {'是' if torch.version.cuda is None else '否'}")
    
    # 检查cuDNN
    try:
        print(f"3. cuDNN版本: {torch.backends.cudnn.version()}")
        print(f"   cuDNN启用: {torch.backends.cudnn.enabled}")
    except:
        print("3. cuDNN版本: 不可用")
    
    # 检查编译信息
    print(f"\n4. PyTorch编译信息:")
    print(f"   - CUDA: {torch.version.cuda}")
    print(f"   - cuDNN: {torch.backends.cudnn.version() if hasattr(torch.backends.cudnn, 'version') else 'N/A'}")
    
    print("\n【问题分析】")
    print("你的系统信息:")
    print("- GPU: NVIDIA GeForce RTX 4070")
    print("- 驱动版本: 561.00")
    print("- 驱动支持的CUDA版本: 12.6")
    print("\n当前PyTorch信息:")
    print(f"- PyTorch版本: {torch.__version__}")
    print(f"- PyTorch CUDA版本: {torch.version.cuda}")
    
    print("\n【可能的问题】")
    if torch.version.cuda is None:
        print("❌ 问题1: 安装的是CPU版本的PyTorch")
    elif torch.version.cuda != '12.6':
        print(f"⚠️  问题2: PyTorch CUDA版本({torch.version.cuda})与驱动CUDA版本(12.6)不匹配")
    
    print("\n【解决方案】")
    print("方案1: 重新安装正确版本的PyTorch")
    print("\n步骤1: 卸载当前的PyTorch")
    print("pip uninstall torch torchvision torchaudio")
    
    print("\n步骤2: 安装CUDA 12.1版本的PyTorch（推荐）")
    print("pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu121")
    
    print("\n或者安装CUDA 12.4版本的PyTorch")
    print("pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu124")
    
    print("\n方案2: 如果方案1不行，尝试安装稳定版本")
    print("pip install torch==2.1.0 torchvision==0.16.0 torchaudio==2.1.0 --index-url https://download.pytorch.org/whl/cu121")
    
    print("\n方案3: 检查CUDA Toolkit")
    print("如果上述方案都不行，可能需要安装CUDA Toolkit:")
    print("1. 从NVIDIA官网下载CUDA Toolkit 12.1或12.4")
    print("2. 安装后重新安装PyTorch")
    
else:
    print("\n✓ CUDA 可用！")
    print(f"2. PyTorch CUDA 版本: {torch.version.cuda}")
    
    gpu_count = torch.cuda.device_count()
    print(f"3. GPU 数量: {gpu_count}")
    
    for i in range(gpu_count):
        print(f"\n   GPU {i}:")
        print(f"   - 名称: {torch.cuda.get_device_name(i)}")
        print(f"   - 计算能力: {torch.cuda.get_device_capability(i)}")
        print(f"   - 总显存: {torch.cuda.get_device_properties(i).total_memory / 1024**3:.2f} GB")
    
    current_device = torch.cuda.current_device()
    print(f"\n4. 当前使用的GPU: {current_device} ({torch.cuda.get_device_name(current_device)})")
    
    print("\n5. 测试GPU计算...")
    try:
        x = torch.randn(1000, 1000).cuda()
        y = torch.randn(1000, 1000).cuda()
        z = torch.matmul(x, y)
        print("   ✓ GPU计算测试成功！")
        print(f"   - 输入张量形状: {x.shape}")
        print(f"   - 输出张量形状: {z.shape}")
        print(f"   - 输出张量设备: {z.device}")
        
        print("\n6. 测试UNet模型在GPU上运行...")
        from unet import SimpleUnet
        model = SimpleUnet().cuda()
        print("   ✓ 模型成功移动到GPU")
        
        img = torch.randn((1, 3, 64, 64)).cuda()
        t = torch.tensor([10]).cuda()
        output = model(img, t)
        print(f"   ✓ 模型前向传播成功！输出形状: {output.shape}")
        
        print(f"\n7. GPU内存使用情况:")
        print(f"   - 已分配: {torch.cuda.memory_allocated() / 1024**2:.2f} MB")
        print(f"   - 已缓存: {torch.cuda.memory_reserved() / 1024**2:.2f} MB")
        
    except Exception as e:
        print(f"   ✗ GPU测试失败: {e}")

print("\n" + "=" * 80)
print("诊断完成！")
print("=" * 80)