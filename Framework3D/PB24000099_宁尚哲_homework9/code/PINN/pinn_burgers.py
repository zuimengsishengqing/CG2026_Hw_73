"""
Simple PINN example solving Burgers' equation using DeepXDE with PyTorch backend.
Usage:
  1) pip install -r requirements_pinn.txt
  2) python pinn_burgers.py

Outputs a small training log and saves model to 'pinn_burgers_model'.
"""
import deepxde as dde
import numpy as np
import matplotlib.pyplot as plt
import torch
 
def main():

    # Burgers equation: u_t + u u_x - (0.01/pi) u_xx = 0
    nu = 0.01 / np.pi

    geom = dde.geometry.Interval(-1, 1)
    timedomain = dde.geometry.TimeDomain(0, 1)
    geomtime = dde.geometry.GeometryXTime(geom, timedomain)

    def pde(x, u):
        # x: [x, t]
        u_t = dde.grad.jacobian(u, x, i=0, j=1)
        u_x = dde.grad.jacobian(u, x, i=0, j=0)
        u_xx = dde.grad.hessian(u, x, i=0, j=0)
        return u_t + u * u_x - nu * u_xx

    def boundary_l(x, on_boundary):
        return on_boundary and np.isclose(x[0], -1)

    def boundary_r(x, on_boundary):
        return on_boundary and np.isclose(x[0], 1)

    bc_l = dde.DirichletBC(geomtime, lambda x: 0, boundary_l)
    bc_r = dde.DirichletBC(geomtime, lambda x: 0, boundary_r)

    def initial_condition(x, on_initial):
        return on_initial and np.isclose(x[1], 0)

    ic = dde.IC(geomtime, lambda x: -np.sin(np.pi * x[:, 0:1]), initial_condition)

    data = dde.data.TimePDE(
        geomtime, pde, [bc_l, bc_r, ic],
        num_domain=2000,
        num_boundary=200,
        num_initial=100,
    )

    net = dde.nn.FNN([2] + [50] * 3 + [1], "tanh", "Glorot normal")
    model = dde.Model(data, net)

    model.compile("adam", lr=1e-3)
    losshistory, train_state = model.train(epochs=2000)

    # Optional: L-BFGS for refinement
    model.compile("L-BFGS")
    losshistory, train_state = model.train()

    # Save model
    model.save("pinn_burgers_model")

    # Predict on a grid and visualize
    x = np.linspace(-1, 1, 100)
    t = np.linspace(0, 1, 100)
    X, T = np.meshgrid(x, t)
    test_x = np.hstack((X.reshape(-1, 1), T.reshape(-1, 1)))

    u_pred = model.predict(test_x).reshape(100, 100)

    # Simple analytical-ish reference (approximate)
    def burgers_analytical(x, t):
        return -np.sin(np.pi * x) * np.exp(-nu * np.pi**2 * t)

    u_analytic = burgers_analytical(X, T)

    plt.figure(figsize=(10, 4))
    plt.subplot(1, 2, 1)
    plt.pcolormesh(T, X, u_pred, cmap="seismic")
    plt.title("PINN Prediction")
    plt.xlabel("t")

    plt.subplot(1, 2, 2)
    plt.pcolormesh(T, X, u_analytic, cmap="seismic")
    plt.title("Analytical (approx)")
    plt.xlabel("t")

    plt.tight_layout()
    plt.savefig("pinn_burgers_result.png")
    print("Saved pinn_burgers_result.png and model files.")

if __name__ == '__main__':
    main()
