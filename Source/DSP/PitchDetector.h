#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace DSP {
namespace PitchDetector {

// McLeod Pitch Method (MPM) implementation
// frame: input frame (typically 512 samples, windowed)
// sampleRate:サンプリング周波数 (16000.0)
// outPitch: output pitch (Hz)
// outClarity: output clarity metric (0.0 to 1.0)
inline bool detectPitchMPM(const float* frame, int frameSize, double sampleRate, float& outPitch, float& outClarity)
{
    // For 16kHz, target range 50Hz to 500Hz:
    // 50Hz  -> tau = 16000 / 50 = 320
    // 500Hz -> tau = 16000 / 500 = 32
    int maxDelay = static_cast<int>(sampleRate / 50.0);
    int minDelay = static_cast<int>(sampleRate / 500.0);

    if (maxDelay >= frameSize / 2) {
        maxDelay = frameSize / 2 - 1;
    }
    if (minDelay < 2) {
        minDelay = 2;
    }

    std::vector<float> nsdf(maxDelay + 1, 0.0f);

    // Compute NSDF
    // n(tau) = 2 * R(tau) / m(tau)
    for (int tau = 0; tau <= maxDelay; ++tau) {
        float r_tau = 0.0f;
        float m_tau = 0.0f;
        
        for (int j = 0; j < frameSize - tau; ++j) {
            r_tau += frame[j] * frame[j + tau];
            m_tau += frame[j] * frame[j] + frame[j + tau] * frame[j + tau];
        }
        
        if (m_tau > 1e-9f) {
            nsdf[tau] = 2.0f * r_tau / m_tau;
        } else {
            nsdf[tau] = 0.0f;
        }
    }

    // Find local maxima (peaks) of NSDF
    struct Peak {
        int index;
        float value;
    };
    std::vector<Peak> peaks;

    for (int tau = minDelay; tau < maxDelay; ++tau) {
        if (nsdf[tau] > nsdf[tau - 1] && nsdf[tau] >= nsdf[tau + 1]) {
            // Found a local peak
            if (nsdf[tau] > 0.0f) {
                peaks.push_back({ tau, nsdf[tau] });
            }
        }
    }

    if (peaks.empty()) {
        outPitch = 0.0f;
        outClarity = 0.0f;
        return false;
    }

    // Find the global maximum peak value
    float max_peak_val = 0.0f;
    for (const auto& p : peaks) {
        if (p.value > max_peak_val) {
            max_peak_val = p.value;
        }
    }

    // According to MPM: select the first peak that is above 0.9 * max_peak_val
    float threshold = 0.85f * max_peak_val;
    int selected_tau_idx = -1;
    float selected_val = 0.0f;

    for (const auto& p : peaks) {
        if (p.value >= threshold) {
            selected_tau_idx = p.index;
            selected_val = p.value;
            break;
        }
    }

    if (selected_tau_idx != -1) {
        // Parabolic Interpolation for sub-sample accuracy
        int alpha_idx = selected_tau_idx - 1;
        int beta_idx = selected_tau_idx;
        int gamma_idx = selected_tau_idx + 1;

        float y1 = nsdf[alpha_idx];
        float y2 = nsdf[beta_idx];
        float y3 = nsdf[gamma_idx];

        float denom = y1 - 2.0f * y2 + y3;
        float delta = 0.0f;
        if (std::abs(denom) > 1e-6f) {
            delta = 0.5f * (y1 - y3) / denom;
        }

        float tau_true = static_cast<float>(selected_tau_idx) + delta;
        outPitch = static_cast<float>(sampleRate / tau_true);
        outClarity = selected_val;
        return true;
    }

    outPitch = 0.0f;
    outClarity = 0.0f;
    return false;
}

// Compute Zero Crossing Rate (ZCR)
inline float computeZcr(const float* frame, int frameSize)
{
    float crossings = 0.0f;
    for (int n = 1; n < frameSize; ++n) {
        float val1 = frame[n - 1];
        float val2 = frame[n];
        // Check sign change
        if ((val1 > 0.0f && val2 < 0.0f) || (val1 < 0.0f && val2 > 0.0f)) {
            crossings += 1.0f;
        }
    }
    return crossings / static_cast<float>(frameSize - 1);
}

// Compute Low-Band Energy Ratio (LBER) using a 1kHz 1-pole Low-pass Filter
inline float computeLber(const float* frame, int frameSize, double sampleRate)
{
    // Cutoff = 1000Hz, SampleRate = 16000Hz
    // fc = 1000 / 16000 = 0.0625
    // omega_c = 2 * pi * fc = 0.3927
    // alpha = exp(-omega_c) = 0.6752
    const float alpha = 0.6752f;
    
    float total_energy = 0.0f;
    float low_energy = 0.0f;
    float filter_state = 0.0f;

    for (int i = 0; i < frameSize; ++i) {
        float x = frame[i];
        total_energy += x * x;

        // Apply low-pass filter: y[n] = (1 - alpha)*x[n] + alpha*y[n-1]
        filter_state = (1.0f - alpha) * x + alpha * filter_state;
        low_energy += filter_state * filter_state;
    }

    if (total_energy > 1e-9f) {
        return low_energy / total_energy;
    }
    return 0.0f;
}

} // namespace PitchDetector
} // namespace DSP
