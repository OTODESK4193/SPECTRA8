#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace DSP {
namespace LspUtils {

// LPC (Linear Predictive Coding) coefficients to LSP (Line Spectral Pairs) conversion
// lpc: pointer to LPC coefficients a_1 to a_order (size: order)
// lsp: output array for LSP coefficients cos(omega_i) (size: order)
// order: LPC analysis order (must be even, typically 16)
inline bool lpcToLsp(const float* lpc, float* lsp, int order)
{
    int M = order / 2;
    std::vector<float> p_prime(M + 1, 0.0f);
    std::vector<float> q_prime(M + 1, 0.0f);

    // a[0] = 1.0, a[1...order] = lpc[0...order-1]
    std::vector<float> a(order + 1, 0.0f);
    a[0] = 1.0f;
    for (int i = 1; i <= order; ++i) {
        a[i] = lpc[i - 1];
    }

    std::vector<float> P(order + 2, 0.0f);
    std::vector<float> Q(order + 2, 0.0f);
    for (int i = 0; i <= order + 1; ++i) {
        float ai = (i <= order) ? a[i] : 0.0f;
        float a_inv = (order + 1 - i <= order) ? a[order + 1 - i] : 0.0f;
        P[i] = ai + a_inv;
        Q[i] = ai - a_inv;
    }

    // Decompose P(z) and Q(z) to P'(z) and Q'(z) by removing trivial roots
    p_prime[0] = P[0];
    q_prime[0] = Q[0];
    for (int i = 1; i <= M; ++i) {
        p_prime[i] = P[i] - p_prime[i - 1];
        q_prime[i] = Q[i] + q_prime[i - 1];
    }

    // Chebyshev coefficients C_i
    std::vector<float> p_coeff(M + 1);
    std::vector<float> q_coeff(M + 1);
    for (int i = 0; i < M; ++i) {
        p_coeff[i] = 2.0f * p_prime[i];
        q_coeff[i] = 2.0f * q_prime[i];
    }
    p_coeff[M] = p_prime[M];
    q_coeff[M] = q_prime[M];

    // Clenshaw's recurrence relation to evaluate Chebyshev polynomial sum
    auto evalChebyshev = [](float x, const float* coeffs, int M_val) {
        float b1 = 0.0f;
        float b2 = 0.0f;
        for (int i = 0; i < M_val; ++i) {
            float b0 = 2.0f * x * b1 - b2 + coeffs[i];
            b2 = b1;
            b1 = b0;
        }
        return x * b1 - b2 + 0.5f * coeffs[M_val];
    };

    // Kabal's method to find roots in [-1, 1] (from 1.0 to -1.0)
    int roots_found = 0;
    float x_prev = 1.0f;
    float y_p_prev = evalChebyshev(x_prev, p_coeff.data(), M);
    float y_q_prev = evalChebyshev(x_prev, q_coeff.data(), M);

    const int num_grid = 128; // Increased resolution for stable root finding
    const float step = 2.0f / num_grid;
    bool search_p = true;

    for (int i = 1; i <= num_grid && roots_found < order; ++i) {
        float x_curr = 1.0f - i * step;
        if (x_curr < -1.0f) x_curr = -1.0f;

        float y_p_curr = evalChebyshev(x_curr, p_coeff.data(), M);
        float y_q_curr = evalChebyshev(x_curr, q_coeff.data(), M);

        if (search_p) {
            if (y_p_prev * y_p_curr <= 0.0f) {
                float xl = x_prev;
                float xr = x_curr;
                float yl = y_p_prev;
                for (int iter = 0; iter < 12; ++iter) { // 12 iterations for high precision
                    float xm = 0.5f * (xl + xr);
                    float ym = evalChebyshev(xm, p_coeff.data(), M);
                    if (yl * ym <= 0.0f) {
                        xr = xm;
                    } else {
                        xl = xm;
                        yl = ym;
                    }
                }
                lsp[roots_found++] = 0.5f * (xl + xr);
                search_p = false;
            }
        } else {
            if (y_q_prev * y_q_curr <= 0.0f) {
                float xl = x_prev;
                float xr = x_curr;
                float yl = y_q_prev;
                for (int iter = 0; iter < 12; ++iter) {
                    float xm = 0.5f * (xl + xr);
                    float ym = evalChebyshev(xm, q_coeff.data(), M);
                    if (yl * ym <= 0.0f) {
                        xr = xm;
                    } else {
                        xl = xm;
                        yl = ym;
                    }
                }
                lsp[roots_found++] = 0.5f * (xl + xr);
                search_p = true;
            }
        }

        x_prev = x_curr;
        y_p_prev = y_p_curr;
        y_q_prev = y_q_curr;
    }

    return (roots_found == order);
}

// LSP coefficients cos(omega_i) to LPC coefficients conversion
inline void lspToLpc(const float* lsp, float* lpc, int order)
{
    int M = order / 2;
    std::vector<float> P(order + 2, 0.0f);
    std::vector<float> Q(order + 2, 0.0f);

    P[0] = 1.0f;
    Q[0] = 1.0f;

    for (int i = 0; i < M; ++i) {
        float p_i = -2.0f * lsp[2 * i];
        float q_i = -2.0f * lsp[2 * i + 1];

        for (int j = 2 * i + 2; j >= 2; --j) {
            P[j] += p_i * P[j - 1] + P[j - 2];
            Q[j] += q_i * Q[j - 1] + Q[j - 2];
        }
        P[1] += p_i * P[0];
        Q[1] += q_i * Q[0];
    }

    for (int j = order + 1; j >= 1; --j) {
        P[j] += P[j - 1];
        Q[j] -= Q[j - 1];
    }

    for (int i = 1; i <= order; ++i) {
        lpc[i - 1] = 0.5f * (P[i] + Q[i]);
    }
}

// Enforce minimum clearance gap between adjacent LSPs in angular domain (acos space)
// This strictly prevents filter explosion / instability
inline void enforceLspClearance(float* lsp, int order)
{
    std::vector<float> w(order);
    for (int i = 0; i < order; ++i) {
        w[i] = std::acos(std::clamp(lsp[i], -0.9999f, 0.9999f));
    }

    std::sort(w.begin(), w.end());

    // min_gap = 0.05 * pi / order
    const float min_gap = 0.05f * 3.14159265f / order;
    
    if (w[0] < min_gap) w[0] = min_gap;
    for (int i = 1; i < order; ++i) {
        if (w[i] - w[i - 1] < min_gap) {
            w[i] = w[i - 1] + min_gap;
        }
    }
    
    if (w[order - 1] > 3.14159265f - min_gap) {
        w[order - 1] = 3.14159265f - min_gap;
        for (int i = order - 2; i >= 0; --i) {
            if (w[i + 1] - w[i] < min_gap) {
                w[i] = w[i + 1] - min_gap;
            }
        }
    }

    for (int i = 0; i < order; ++i) {
        lsp[i] = std::cos(w[i]);
    }
}

} // namespace LspUtils
} // namespace DSP
