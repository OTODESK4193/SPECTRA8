# =====================================================================
# SPECTRA8 フェーズ2 M0: LPC分析→ラティス合成 Pythonプロトタイプ
# 計画書 v2 §5 の数理仕様に厳密準拠（符号規約: A(z) = 1 + Σ a_i z^-i）
#
# Done条件:
#  (1) 合成母音/a/ (F1=730, F2=1090, F3=2440Hz) の推定フォルマント ±5%以内
#  (2) ホワイトノイズ長時間入力で NaN ゼロ
#  (3) 予測ゲイン > 10dB (有声)
#  (4) ラティス再合成でフォルマントが保存される（追加サニティ）
#  (5) 極端パラメータ連打でも発散しない（追加ストレス）
# =====================================================================
import numpy as np

FS = 16000
N_WIN = 256          # 窓長 16ms @16kHz
HOP = 320            # 既定 FRAME RATE 50Hz
K_CLAMP = 0.995
SIGMA_LAG = 50.0     # ラグ窓 σ=50Hz
CTRL_BLOCK = 32      # コントロールブロック（補間単位）
SAT = 8.0            # ラティス状態飽和リミット（時変k時の発散防止、通常動作では不介入）

# ---------------------------------------------------------------------
# §5.2 分析
# ---------------------------------------------------------------------
def make_window(n, kind="hann"):
    t = np.arange(n)
    if kind == "hann":
        return 0.5 - 0.5 * np.cos(2 * np.pi * t / (n - 1))
    if kind == "hamming":
        return 0.54 - 0.46 * np.cos(2 * np.pi * t / (n - 1))
    if kind == "blackman":
        return 0.42 - 0.5 * np.cos(2 * np.pi * t / (n - 1)) + 0.08 * np.cos(4 * np.pi * t / (n - 1))
    raise ValueError(kind)

def autocorr(xw, P):
    r = np.empty(P + 1)
    for j in range(P + 1):
        r[j] = np.dot(xw[: len(xw) - j], xw[j:])
    return r

def lag_window(r, sigma=SIGMA_LAG, fs=FS):
    j = np.arange(len(r))
    return r * np.exp(-0.5 * (2 * np.pi * sigma * j / fs) ** 2)

def levinson(r, P):
    """Levinson-Durbin。戻り値: k[1..P](長さP), a[1..P](長さP, A(z)=1+Σa_i z^-i), E_P
       kクランプ ±0.995、E<1e-9 で以降 k=0 打ち切り。"""
    a = np.zeros(P + 1)
    k = np.zeros(P)
    E = r[0]
    if E <= 1e-9:
        return k, a[1:], 0.0
    for i in range(1, P + 1):
        acc = r[i]
        for j in range(1, i):
            acc += a[j] * r[i - j]
        ki = -acc / E
        ki = min(max(ki, -K_CLAMP), K_CLAMP)
        k[i - 1] = ki
        a_new = a.copy()
        a_new[i] = ki
        for j in range(1, i):
            a_new[j] = a[j] + ki * a[i - j]
        a = a_new
        E = E * (1.0 - ki * ki)
        if E < 1e-9:
            E = max(E, 0.0)
            break  # 以降 k=0（初期値のまま）
    return k, a[1:], E

def analyze_frame(frame, P, window="hann", silence_thresh=1e-7):
    """§5.2: 窓→自己相関→数値衛生→Levinson。戻り値 (k, a, G)"""
    w = make_window(len(frame), window)
    xw = frame * w
    r = autocorr(xw, P)
    if r[0] < silence_thresh:                      # §5.2-6 無音フレーム
        return np.zeros(P), np.zeros(P), 0.0
    r[0] = r[0] * 1.0001 + 1e-9                    # 白色雑音補正
    r = lag_window(r)
    k, a, E = levinson(r, P)
    G = np.sqrt(max(E, 0.0))
    return k, a, G

# ---------------------------------------------------------------------
# §5.3 合成（ラティス、毎サンプル）
# ---------------------------------------------------------------------
def synthesize(excitation, frames_k, frames_G, P, hop=HOP):
    """フレーム毎ターゲット (k,G) をコントロールブロック(32smp)毎に線形補間して合成。
       §5.3 のラティス漸化式を純Python floatで実装（速度対策）。"""
    exc = [float(v) for v in excitation]
    n = len(exc)
    out = [0.0] * n
    b = [0.0] * (P + 1)
    k_cur = [0.0] * P
    G_cur = 0.0
    fk = [[float(v) for v in kk] for kk in frames_k]
    fG = [float(g) for g in frames_G]
    for blk_start in range(0, n, CTRL_BLOCK):
        fi = min(blk_start // hop, len(fk) - 1)
        k_tgt = fk[fi]; G_tgt = fG[fi]
        blk_end = min(blk_start + CTRL_BLOCK, n)
        m = blk_end - blk_start
        # ブロック内線形ランプの増分（|k|<1 は線形補間途中でも保存される）
        dk = [(k_tgt[i] - k_cur[i]) / m for i in range(P)]
        dG = (G_tgt - G_cur) / m
        k = k_cur[:]
        G = G_cur
        for i in range(m):
            for p in range(P):
                k[p] += dk[p]
            G += dG
            f = G * exc[blk_start + i]
            for p in range(P, 0, -1):
                kp = k[p - 1]
                f -= kp * b[p - 1]
                bn = kp * f + b[p - 1]
                # 状態飽和 (TMS5220流): 時変kでのエネルギーポンピングを構造的に遮断
                b[p] = SAT if bn > SAT else (-SAT if bn < -SAT else bn)
            f = SAT if f > SAT else (-SAT if f < -SAT else f)
            b[0] = f + 1e-20   # デノーマル対策注入（§5.3）
            out[blk_start + i] = f
        k_cur = k_tgt[:]; G_cur = G_tgt
    return np.array(out)

# ---------------------------------------------------------------------
# テスト信号
# ---------------------------------------------------------------------
def resonator_coeffs(f, bw, fs=FS):
    r = np.exp(-np.pi * bw / fs)
    theta = 2 * np.pi * f / fs
    return r, theta

def synth_vowel_a(dur=1.0, f0=100.0, fs=FS):
    """/a/: F1=730, F2=1090, F3=2440Hz を2次共振器カスケードで生成"""
    n = int(dur * fs)
    exc = np.zeros(n)
    period = fs / f0
    t = 0.0
    while t < n:
        exc[int(t)] = 1.0
        t += period
    y = exc.copy()
    for f, bw in [(730, 90), (1090, 110), (2440, 170)]:
        r, th = resonator_coeffs(f, bw)
        a1, a2 = -2 * r * np.cos(th), r * r
        out = np.zeros(n)
        for i in range(n):
            out[i] = y[i] - a1 * (out[i - 1] if i >= 1 else 0) - a2 * (out[i - 2] if i >= 2 else 0)
        y = out
    return y / np.max(np.abs(y))

def formants_from_a(a, fs=FS, min_bw=700.0):
    """A(z)=1+Σa_i z^-i の根からフォルマント推定（検証専用。合成には使わない）"""
    poly = np.concatenate(([1.0], a))
    roots = np.roots(poly)
    roots = roots[np.imag(roots) > 0.01]
    freqs = np.angle(roots) * fs / (2 * np.pi)
    bws = -np.log(np.abs(roots)) * fs / np.pi
    sel = (bws < min_bw) & (freqs > 90) & (freqs < fs / 2 - 200)
    return np.sort(freqs[sel])

# ---------------------------------------------------------------------
# M0 テスト本体
# ---------------------------------------------------------------------
def main():
    rng = np.random.default_rng(42)
    P = 16

    # ===== テスト1: フォルマント推定精度 =====
    vowel = synth_vowel_a(1.0)
    targets = np.array([730.0, 1090.0, 2440.0])
    errs = []
    frames_k, frames_a, frames_G = [], [], []
    for start in range(0, len(vowel) - N_WIN, HOP):
        fr = vowel[start:start + N_WIN]
        k, a, G = analyze_frame(fr, P)
        frames_k.append(k); frames_a.append(a); frames_G.append(G)
    for a in frames_a[20:40]:
        f = formants_from_a(a)
        if len(f) >= 3:
            est = np.array([f[np.argmin(np.abs(f - t))] for t in targets])
            errs.append(np.abs(est - targets) / targets * 100)
    errs = np.array(errs)
    mean_err = errs.mean(axis=0)
    ok1 = np.all(mean_err < 5.0)
    print(f"[T1] フォルマント誤差% F1/F2/F3 = {mean_err.round(2)}  -> {'PASS' if ok1 else 'FAIL'}")

    # ===== テスト2: 予測ゲイン (有声) =====
    pg = []
    for start in range(0, len(vowel) - N_WIN, HOP):
        fr = vowel[start:start + N_WIN]
        w = make_window(N_WIN)
        xw = fr * w
        r = autocorr(xw, P)
        if r[0] < 1e-7: continue
        r0 = r[0]
        r[0] = r[0] * 1.0001 + 1e-9
        r = lag_window(r)
        _, _, E = levinson(r, P)
        if E > 0:
            pg.append(10 * np.log10(r0 / E))
    pg_med = np.median(pg)
    ok2 = pg_med > 10.0
    print(f"[T2] 予測ゲイン中央値 = {pg_med:.1f} dB  -> {'PASS' if ok2 else 'FAIL'}")

    # ===== テスト3: ラティス再合成でフォルマント保存 =====
    n = len(vowel)
    period = FS / 100.0
    exc = np.zeros(n); t = 0.0
    while t < n:
        exc[int(t)] = 1.0; t += period
    resyn = synthesize(exc, frames_k, frames_G, P)
    assert np.all(np.isfinite(resyn)), "再合成にNaN/Inf"
    errs2 = []
    for start in range(20 * HOP, 40 * HOP, HOP):
        _, a, _ = analyze_frame(resyn[start:start + N_WIN], P)
        f = formants_from_a(a)
        if len(f) >= 3:
            est = np.array([f[np.argmin(np.abs(f - t0))] for t0 in targets])
            errs2.append(np.abs(est - targets) / targets * 100)
    mean_err2 = np.array(errs2).mean(axis=0)
    ok3 = np.all(mean_err2 < 5.0)
    print(f"[T3] 再合成後フォルマント誤差% = {mean_err2.round(2)}, ピーク絶対値 = {np.max(np.abs(resyn)):.3f}  -> {'PASS' if ok3 else 'FAIL'}")

    # ===== テスト4: ホワイトノイズ長時間 分析NaNチェック =====
    dur_s = 600  # 10分
    nan_found = False
    all_k_ok = True
    chunk = FS * 10
    carry = np.zeros(0)
    total_frames = 0
    for c in range(dur_s // 10):
        noise = rng.standard_normal(chunk) * 0.3
        x = np.concatenate([carry, noise])
        nf = (len(x) - N_WIN) // HOP
        for i in range(nf):
            fr = x[i * HOP: i * HOP + N_WIN]
            k, a, G = analyze_frame(fr, P)
            total_frames += 1
            if not (np.all(np.isfinite(k)) and np.isfinite(G)):
                nan_found = True
            if np.any(np.abs(k) > K_CLAMP + 1e-12):
                all_k_ok = False
        carry = x[nf * HOP:]
    ok4 = (not nan_found) and all_k_ok
    print(f"[T4] ノイズ{dur_s}s 分析 {total_frames}フレーム: NaN={nan_found}, 全|k|<=0.995={all_k_ok}  -> {'PASS' if ok4 else 'FAIL'}")

    # ===== テスト5: 合成ストレス（極端パラメータ連打・60s＋敵対的k） =====
    n5 = FS * 60
    exc5 = rng.standard_normal(n5)
    nfr = n5 // HOP + 1
    fk = [np.clip(rng.uniform(-1.2, 1.2, P), -K_CLAMP, K_CLAMP) for _ in range(nfr)]
    fG = [rng.uniform(0.0, 2.0) for _ in range(nfr)]
    out5 = synthesize(exc5, fk, fG, P)
    peak5 = np.max(np.abs(out5))
    ok5 = np.all(np.isfinite(out5)) and peak5 <= SAT + 1e-9
    print(f"[T5] 敵対的k連打 60s 合成: NaN/Inf={'なし' if np.all(np.isfinite(out5)) else 'あり'}, ピーク={peak5:.2f}  -> {'PASS' if ok5 else 'FAIL'}")

    # ===== テスト6: 無音・DC入力 =====
    sil_k, sil_a, sil_G = analyze_frame(np.zeros(N_WIN), P)
    dc_k, dc_a, dc_G = analyze_frame(np.ones(N_WIN) * 0.5, P)
    ok6 = np.all(sil_k == 0) and sil_G == 0 and np.all(np.isfinite(dc_k)) and np.isfinite(dc_G)
    print(f"[T6] 無音: k全0={np.all(sil_k == 0)}, G=0={sil_G == 0} / DC: 有限={np.all(np.isfinite(dc_k))}  -> {'PASS' if ok6 else 'FAIL'}")

    # ===== ゴールデンテストデータ出力 (M1 C++検証用) =====
    golden_frames = []
    gsrc = [vowel[25 * HOP:25 * HOP + N_WIN], vowel[30 * HOP:30 * HOP + N_WIN], vowel[35 * HOP:35 * HOP + N_WIN],
            rng.standard_normal(N_WIN) * 0.3, rng.standard_normal(N_WIN) * 0.01,
            np.zeros(N_WIN), np.sin(2 * np.pi * 440 * np.arange(N_WIN) / FS) * 0.8]
    for win in ["hann", "hamming", "blackman"]:
        for order in [8, 10, 12, 16]:
            for fi, fr in enumerate(gsrc):
                k, a, G = analyze_frame(np.asarray(fr, dtype=np.float64), order, win)
                golden_frames.append((win, order, fi, fr, k, G))
    with open("golden_lpc.csv", "w") as f:
        f.write("# window,order,frame_id,then N_WIN samples, then k[order], then G\n")
        for win, order, fi, fr, k, G in golden_frames:
            f.write(f"{win},{order},{fi},")
            f.write(",".join(f"{v:.9e}" for v in fr) + ",")
            f.write(",".join(f"{v:.9e}" for v in k) + ",")
            f.write(f"{G:.9e}\n")
    print(f"[GOLDEN] golden_lpc.csv 出力: {len(golden_frames)}ケース")

    all_ok = ok1 and ok2 and ok3 and ok4 and ok5 and ok6
    print("=" * 50)
    print("M0 総合判定:", "PASS" if all_ok else "FAIL")
    return 0 if all_ok else 1

if __name__ == "__main__":
    raise SystemExit(main())
