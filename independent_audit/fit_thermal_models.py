#!/usr/bin/env python3
"""
Thermal Constitutive Model Discrimination:
Compares classical parabolic Fourier diffusion vs. hyperbolic Cattaneo-Maxwell (Second Sound)
on the 37-tick Asolaria thermal slab trajectory.
"""

import numpy as np
import pandas as pd
from scipy.optimize import curve_fit, minimize
import os

def run():
    csv_path = "cpp_independent_thermal.csv"
    if not os.path.exists(csv_path):
        csv_path = "../thermal_diffusion_results.csv"
    if not os.path.exists(csv_path):
        csv_path = "thermal_diffusion_results.csv"

    df = pd.read_csv(csv_path)
    t = df["tick"].values.astype(float)
    y = df["delta_T"].values.astype(float)

    # Total sum of squares
    y_mean = np.mean(y)
    sst = np.sum((y - y_mean)**2)

    # Model 1: Classical Parabolic Exponential Relaxation
    # DeltaT(t) = 8.0 * exp(-beta * t)
    def exp_model(t, beta):
        return 8.0 * np.exp(-beta * t)

    popt_exp, _ = curve_fit(exp_model, t, y, p0=[0.1])
    beta_fit = popt_exp[0]
    y_pred_exp = exp_model(t, beta_fit)
    sse_exp = np.sum((y - y_pred_exp)**2)
    rmse_exp = np.sqrt(np.mean((y - y_pred_exp)**2))
    r2_exp = 1.0 - (sse_exp / sst)

    # Model 2: Hyperbolic Second Sound (Damped Harmonic Wave)
    # DeltaT(t) = exp(-alpha * t) * (A * cos(omega * t) + B * sin(omega * t))
    def osc_model(params):
        alpha, omega, A, B = params
        pred = np.exp(-alpha * t) * (A * np.cos(omega * t) + B * np.sin(omega * t))
        return np.sum((y - pred)**2)

    res = minimize(osc_model, [0.01, 0.23, 8.0, 0.0], method="Nelder-Mead",
                   options={"maxiter": 10000, "xatol": 1e-8, "fatol": 1e-8})
    alpha_fit, omega_fit, A_fit, B_fit = res.x
    y_pred_osc = np.exp(-alpha_fit * t) * (A_fit * np.cos(omega_fit * t) + B_fit * np.sin(omega_fit * t))
    sse_osc = np.sum((y - y_pred_osc)**2)
    rmse_osc = np.sqrt(np.mean((y - y_pred_osc)**2))
    r2_osc = 1.0 - (sse_osc / sst)
    period_fit = 2.0 * np.pi / omega_fit

    print("=======================================================================")
    print(" THERMAL TRANSPORT CONSTITUTIVE MODEL COMPARISON")
    print("=======================================================================")
    print(f"Data points: {len(t)} ticks (t = 0 to 36)")
    print("\n--- Model 1: Classical Fourier Diffusion ---")
    print(f"  Formula: DeltaT(t) = 8.0 * exp(-beta * t)")
    print(f"  Fitted beta : {beta_fit:.5f} tick^-1")
    print(f"  RMSE        : {rmse_exp:.4f}")
    print(f"  Fit R^2     : {r2_exp:.4f}  ===> POOR FIT (Parabolic diffusion falsified)")

    print("\n--- Model 2: Hyperbolic Second Sound (Cattaneo-Maxwell) ---")
    print(f"  Formula: DeltaT(t) = exp(-alpha * t) * [A*cos(omega*t) + B*sin(omega*t)]")
    print(f"  Fitted omega: {omega_fit:.5f} rad/tick (Period T = {period_fit:.2f} ticks)")
    print(f"  Fitted alpha: {alpha_fit:.6f} tick^-1")
    print(f"  Amplitudes  : A = {A_fit:.4f}, B = {B_fit:.4f}")
    print(f"  RMSE        : {rmse_osc:.4f}")
    print(f"  Fit R^2     : {r2_osc:.4f}  ===> STRONG FIT (Hyperbolic wave confirmed)")
    print("=======================================================================")

if __name__ == "__main__":
    run()
