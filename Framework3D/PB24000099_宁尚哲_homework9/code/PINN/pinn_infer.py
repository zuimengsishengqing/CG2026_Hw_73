"""
Generic PyTorch model inference utility.
Usage:
    python pinn_infer.py <model_path> <input_csv> <output_csv>

Input CSV: NxD (comma-separated) features per row.
Output CSV: NxM predictions per row (comma-separated).

The model is expected to accept a FloatTensor of shape (N, D) and output (N, M).
"""
import sys
import numpy as np
import torch


def main():
    if len(sys.argv) < 4:
        print("Usage: python pinn_infer.py <model_path> <input_csv> <output_csv>")
        return
    model_path = sys.argv[1]
    input_csv = sys.argv[2]
    output_csv = sys.argv[3]

    # Load input
    data = np.loadtxt(input_csv, delimiter=',')
    if data.ndim == 1:
        data = data.reshape(-1, 1)

    # Load model
    device = torch.device('cpu')
    try:
        model = torch.load(model_path, map_location=device)
    except Exception as e:
        # Try loading as scripted model
        model = torch.jit.load(model_path, map_location=device)

    model.eval()

    x = torch.from_numpy(data.astype(np.float32)).to(device)
    with torch.no_grad():
        out = model(x)
        out_np = out.cpu().numpy()

    np.savetxt(output_csv, out_np, delimiter=',')
    print(f"Saved predictions to {output_csv}")

if __name__ == '__main__':
    main()
