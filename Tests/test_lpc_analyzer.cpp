// ==========================================
// File: test_lpc_analyzer.cpp
// M1 ゴールデンテスト: Pythonプロトタイプ(M0)の出力と C++ LpcAnalyzer の
// k係数が 1e-4 以内で一致することを検証する（計画書v2 §6 M1 Done条件）
//
// ビルド (スタブ環境):
//   g++ -O2 -std=c++17 -I../Source/DSP test_lpc_analyzer.cpp ../Source/DSP/LpcAnalyzer.cpp -o test_lpc
//   ./test_lpc golden_lpc.csv
// ==========================================
#include "LpcAnalyzer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const char* path = (argc > 1) ? argv[1] : "golden_lpc.csv";
    std::ifstream in(path);
    if (!in)
    {
        std::printf("golden file not found: %s\n", path);
        return 2;
    }

    LpcAnalyzer analyzer;
    analyzer.prepare();

    const double kTol = 1e-4;
    int cases = 0, failures = 0;
    double maxKErr = 0.0, maxGErr = 0.0;

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> tok;
        {
            std::stringstream ss(line);
            std::string t;
            while (std::getline(ss, t, ','))
                tok.push_back(t);
        }

        const std::string& win = tok[0];
        const int order = std::atoi(tok[1].c_str());
        const size_t need = 3 + (size_t)LpcAnalyzer::kWindowSize + (size_t)order + 1;
        if (tok.size() != need)
        {
            std::printf("CSV parse error: got %zu tokens, want %zu\n", tok.size(), need);
            return 2;
        }

        float frame[LpcAnalyzer::kWindowSize];
        for (int i = 0; i < LpcAnalyzer::kWindowSize; ++i)
            frame[i] = (float)std::atof(tok[(size_t)(3 + i)].c_str());

        std::vector<double> kExp((size_t)order);
        for (int i = 0; i < order; ++i)
            kExp[(size_t)i] = std::atof(tok[(size_t)(3 + LpcAnalyzer::kWindowSize + i)].c_str());
        const double gExp = std::atof(tok.back().c_str());

        int wt = (win == "hann") ? 0 : (win == "hamming") ? 1 : 2;
        analyzer.setWindowType(wt);

        float kGot[LpcAnalyzer::kMaxOrder] = {};
        const float gGot = analyzer.analyzeFrame(frame, order, kGot);

        bool ok = true;
        for (int i = 0; i < order; ++i)
        {
            const double e = std::fabs((double)kGot[i] - kExp[(size_t)i]);
            maxKErr = std::max(maxKErr, e);
            if (e > kTol) ok = false;
            if (std::fabs((double)kGot[i]) > 0.995 + 1e-9) ok = false;   // 全kが±0.995内
            if (!std::isfinite(kGot[i])) ok = false;
        }
        {
            const double e = std::fabs((double)gGot - gExp) / std::max(1.0, std::fabs(gExp));
            maxGErr = std::max(maxGErr, e);
            if (e > kTol) ok = false;
            if (!std::isfinite(gGot)) ok = false;
        }

        ++cases;
        if (!ok)
        {
            ++failures;
            std::printf("FAIL case %d (win=%s order=%d)\n", cases, win.c_str(), order);
        }
    }

    std::printf("cases=%d failures=%d maxKerr=%.3e maxGerr(rel)=%.3e\n",
                cases, failures, maxKErr, maxGErr);
    std::printf(failures == 0 ? "M1 GOLDEN TEST: PASS\n" : "M1 GOLDEN TEST: FAIL\n");
    return failures == 0 ? 0 : 1;
}
