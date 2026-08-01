# 検証用シミュレーション

JUCE 本体が無い環境でも DSP を数値検証するための一式です。

## C++ ハーネス (実コードをリンクして測る)
`juce/JuceHeader.h` は構文チェック専用の最小スタブです。リポジトリの Source/ を
そのままコンパイルできます。

```sh
# Source/ をこのフォルダの隣にコピーしてから:
g++ -std=c++20 -O2 -I juce -I Source -I Source/DSP \
    verify.cpp Source/DSP/FilterbankVocoder.cpp Source/DSP/LpcVocoder.cpp \
    Source/DSP/LpcAnalyzer.cpp -o verify && ./verify
```

| ファイル | 測るもの |
|---|---|
| `verify.cpp` | Filterbank の出力レベル / 0dBFS超え率 / 自己ボコードのユニティ / BANDS切替の不連続量 / LPC ORDER別レベル |
| `buzz2.cpp` | 検波リップル（ビビり音の直接指標）。`analyzeForMeter` 経由でエンベロープを読む |
| `verify3.cpp` | LFO の実測レート / ENV LOOP の周期 / rate=NaN でフリーズしないか |
| `sweep.cpp` | 検波の att/rel 定数の掃引。ビビりと立ち上がり時間のトレードオフを出す（自己完結・JUCE不要） |

## Python (設計時の探索用)
`fblib.py` に FilterbankVocoder を再現。`fb2/fb3/fb4.py` がレベル・バンド数補償・改修案の検証、
`lpc.py` / `lpc2.py` が LPC の ORDER 別レベルとゲイン方式の比較、`lfo.py` が LFO レート誤差。
