# LFOレート誤差の検証
for host in (44100.0, 48000.0, 88200.0, 96000.0):
    # 制御ティックは 16kHz ストリームの32サンプル毎 = 500回/秒
    ticks = 16000.0/32.0
    # 1ティックあたりの位相進み = freq*32/host
    for f in (1.0, 5.0):
        actual = ticks * f*32.0/host
        print(f"host={host/1000:5.1f}kHz  設定{f:4.1f}Hz → 実測 {actual:6.3f}Hz  (×{actual/f:.3f})")
    print()
