from fblib import *
import numpy as np
N=16000; mod=voice(N)*0.9
print("=== バンド数補償則の実測 (現行 makeup を割り戻した素の合成レベル) ===")
base={}
for b in (8,12,16,24,32,48):
    for carf,lbl in ((130.0,'Saw130'),(220.0,'Saw220')):
        car=saw(carf,N)
        L,R,ET,f0=run2(mod,car,bands=b)
        base.setdefault(lbl,{})[b]=np.sqrt(((L**2+R**2)/2).mean())/(32.36*np.sqrt(48.0/b))
for lbl,d in base.items():
    r48=d[48]
    print(f"  {lbl}: "+"  ".join(f"b={b}:{20*np.log10(d[b]/r48):+5.1f}dB" for b in sorted(d)))
print("  (現行 sqrt(48/b) は b=8 で +7.8dB 足している)")
print("\n=== 既定設定で peak = -6 dBFS にするための makeup 逆算 ===")
car=saw(130.0,N)
for b in (48,24,8):
    L,R,ET,f0=run2(mod,car,bands=b)
    pk=max(np.abs(L).max(),np.abs(R).max()); need=10**(-6/20)/pk
    print(f"  BANDS={b:2d}: peak={20*np.log10(pk):+6.2f}dB → ×{need:.3f}({20*np.log10(need):+.1f}dB), 実効makeup={32.36*np.sqrt(48/b)*need:.2f}")
print("\n=== 自己ボコード(キャリア=モジュレーター)のユニティ検証 ===")
for b in (48,24,8):
    L,R,ET,f0=run2(mod,mod,bands=b)
    print(f"  BANDS={b:2d}: in={20*np.log10(np.sqrt((mod**2).mean())):+6.2f}dB out={20*np.log10(np.sqrt(((L**2+R**2)/2).mean())):+6.2f}dB peak={20*np.log10(max(np.abs(L).max(),np.abs(R).max())):+6.2f}dB")
print("\n=== 交互ハードパンの影響 (stereoWidth=1.0固定) ===")
L,R,ET,f0=run2(mod,saw(130.0,N),bands=48,width=1.0)
Lm,Rm,_,_=run2(mod,saw(130.0,N),bands=48,width=0.0)
corr=np.corrcoef(L,R)[0,1]
print(f"  width=1.0: L/R相関={corr:+.3f}  モノ和peak={20*np.log10(np.abs((L+R)/2).max()):+6.2f}dB  L単独peak={20*np.log10(np.abs(L).max()):+6.2f}dB")
print(f"  width=0.0: peak={20*np.log10(np.abs(Lm).max()):+6.2f}dB  (センターのみ)")
