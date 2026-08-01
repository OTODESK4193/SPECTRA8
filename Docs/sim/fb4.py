from fblib import *
import numpy as np
# バンド毎リリース時定数を導入した改良版エンベロープ検出
def run3(mod,car,bands=48,character=1.0,reso=1.0,width=0.6,makeup=8.0,bandcomp=False,perband=True):
    f0,Qb=layout(bands); Q=np.clip(Qb*reso,1.0,40.0)
    ga,_,a1a=coeffs(f0,Q); gs,_,a1s=coeffs(np.clip(f0,50,7800),Q)
    N=len(mod); z=lambda:np.zeros(bands)
    s1a,s2a,s1b,s2b=z(),z(),z(),z(); p1,p2,q1,q2=z(),z(),z(),z(); env=z()
    if perband:
        # att: 0.5〜3ms  rel: max(character依存, 2.5周期/f0)  → 基音リップルを除去
        attT=np.full(bands,0.0005+ (1.0-character)*0.010)
        relT=np.maximum(0.008+(1.0-character)*0.060, 2.5/np.maximum(f0*0.35,60.0))
        att=1-np.exp(-1/(attT*SR)); rel=1-np.exp(-1/(relT*SR))
    else:
        att=np.full(bands,0.005+character*0.045); rel=np.full(bands,0.001+character*0.009)
    invQ2=1/(Q*Q)
    bws=np.clip((f0-80)/120,0,1); th=np.pi/4+np.where(np.arange(bands)%2==0,1,-1)*width*bws*np.pi/4
    pL,pR=np.cos(th),np.sin(th)
    mk=makeup*(np.sqrt(48.0/bands) if bandcomp else 1.0)
    oL=np.zeros(N);oR=np.zeros(N);ET=np.zeros((N,bands))
    for n in range(N):
        x=mod[n]
        v=a1a*(s1a+ga*(x-s2a));bp=v;lp=s2a+ga*v;s1a=2*bp-s1a;s2a=2*lp-s2a
        v2=a1a*(s1b+ga*(bp-s2b));bp2=v2;lp2=s2b+ga*v2;s1b=2*bp2-s1b;s2b=2*lp2-s2b
        e=np.abs(bp2/(Q*Q)); env+=np.where(e>env,att,rel)*(e-env); ET[n]=env
        c=car[n]
        w=a1s*(p1+gs*(c-p2));wb=w;wl=p2+gs*w;p1=2*wb-p1;p2=2*wl-p2
        w2=a1s*(q1+gs*(wb-q2));wb2=w2;wl2=q2+gs*w2;q1=2*wb2-q1;q2=2*wl2-q2
        co=wb2*invQ2*env
        oL[n]=np.sum(co*pL)*mk; oR[n]=np.sum(co*pR)*mk
    return oL,oR,ET,f0
N=16000; mod=voice(N)*0.9; car=saw(130.0,N)
print("=== 改修案の検証: makeup 32.36→8.0 / bandComp撤廃 / バンド毎リリース / width 1.0→0.6 ===")
print(f"{'BANDS':>6} {'現行peak':>10} {'改修peak':>10} {'現行リップル':>12} {'改修リップル':>12}")
for b in (48,32,24,16,8):
    L0,R0,E0,f0=run2(mod,car,bands=b)
    L1,R1,E1,_ =run3(mod,car,bands=b)
    idx=[i for i in range(b) if f0[i]<400][:5]
    rip=lambda E:np.mean([(E[8000:,i].max()-E[8000:,i].min())/(E[8000:,i].mean()+1e-12) for i in idx])
    print(f"{b:>6} {20*np.log10(max(np.abs(L0).max(),np.abs(R0).max())):>+9.2f}dB {20*np.log10(max(np.abs(L1).max(),np.abs(R1).max())):>+9.2f}dB {rip(E0):>12.2f} {rip(E1):>12.2f}")
print("\n=== 改修後の自己ボコード・ユニティ ===")
for b in (48,24,8):
    L,R,E,_=run3(mod,mod,bands=b)
    print(f"  BANDS={b:2d}: in={20*np.log10(np.sqrt((mod**2).mean())):+6.2f}dB out={20*np.log10(np.sqrt(((L**2+R**2)/2).mean())):+6.2f}dB peak={20*np.log10(max(np.abs(L).max(),np.abs(R).max())):+6.2f}dB")
print("\n=== 改修後のCHARACTER応答(明瞭度は保たれるか: 音節レートの追従を確認) ===")
for ch in (0.0,0.5,1.0):
    L,R,E,f0=run3(mod,car,bands=48,character=ch)
    idx=[i for i in range(48) if f0[i]<400][:5]
    rip=np.mean([(E[8000:,i].max()-E[8000:,i].min())/(E[8000:,i].mean()+1e-12) for i in idx])
    hi=[i for i in range(48) if f0[i]>2000][:5]
    riph=np.mean([(E[8000:,i].max()-E[8000:,i].min())/(E[8000:,i].mean()+1e-12) for i in hi])
    print(f"  CHAR={ch:.1f}: 低域リップル={rip:.2f} 高域リップル={riph:.2f} peak={20*np.log10(max(np.abs(L).max(),np.abs(R).max())):+6.2f}dB")
