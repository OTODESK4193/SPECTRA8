exec(open('fb.py').read().split("N=16000")[0])
import numpy as np
def run2(mod,car,bands=48,character=1.0,reso=1.0,width=1.0):
    f0,Qb=layout(bands); Q=np.clip(Qb*reso,1.0,40.0)
    ga,ka,a1a=coeffs(f0,Q); gs,ks,a1s=coeffs(np.clip(f0,50,7800),Q)
    N=len(mod); z=lambda:np.zeros(bands)
    s1a,s2a,s1b,s2b=z(),z(),z(),z(); p1,p2,q1,q2=z(),z(),z(),z(); env=z()
    att=0.005+character*0.045; rel=0.001+character*0.009; invQ2=1/(Q*Q)
    bws=np.clip((f0-80)/120,0,1); th=np.pi/4+np.where(np.arange(bands)%2==0,1,-1)*width*bws*np.pi/4
    pL,pR=np.cos(th),np.sin(th)
    mk=32.36*np.sqrt(48.0/bands)
    oL=np.zeros(N);oR=np.zeros(N);ET=np.zeros((N,bands))
    for n in range(N):
        x=mod[n]
        v=a1a*(s1a+ga*(x-s2a));bp=v;lp=s2a+ga*v;s1a=2*bp-s1a;s2a=2*lp-s2a
        v2=a1a*(s1b+ga*(bp-s2b));bp2=v2;lp2=s2b+ga*v2;s1b=2*bp2-s1b;s2b=2*lp2-s2b
        an=bp2/(Q*Q); e=np.abs(an); env+=np.where(e>env,att,rel)*(e-env); ET[n]=env
        c=car[n]
        w=a1s*(p1+gs*(c-p2));wb=w;wl=p2+gs*w;p1=2*wb-p1;p2=2*wl-p2
        w2=a1s*(q1+gs*(wb-q2));wb2=w2;wl2=q2+gs*w2;q1=2*wb2-q1;q2=2*wl2-q2
        co=wb2*invQ2*env
        oL[n]=np.sum(co*pL)*mk; oR[n]=np.sum(co*pR)*mk
    return oL,oR,ET,f0
N=16000; mod=voice(N)*0.9; car=saw(130.0,N)
print("=== 実チェーン(パン込, stereoWidth=1.0固定) 入力 -0.9dBFS 母音, キャリア=Saw130Hz ===")
for b in (48,32,24,16,8):
    L,R,ET,f0=run2(mod,car,bands=b)
    pk=max(np.abs(L).max(),np.abs(R).max())
    print(f" BANDS={b:2d}  peak={20*np.log10(pk):+6.2f} dBFS   rms(L)={20*np.log10(np.sqrt((L**2).mean())):+6.2f} dB   over0dB={100*np.mean(np.maximum(np.abs(L),np.abs(R))>1.0):5.1f}%")
print()
print("=== CHARACTER によるエンベロープ・リップル(=ビビり音の指標) ===")
print("  低域バンド(f0≈150Hz)のenvelopeの基音周期リップル深さ")
for ch in (0.0,0.25,0.5,0.75,1.0):
    L,R,ET,f0=run2(mod,car,bands=48,character=ch)
    seg=ET[8000:16000]
    idx=[i for i in range(48) if f0[i]<400][:6]
    rip=[(seg[:,i].max()-seg[:,i].min())/(seg[:,i].mean()+1e-12) for i in idx]
    pk=max(np.abs(L).max(),np.abs(R).max())
    print(f"  CHARACTER={ch:.2f}  リップル比={np.mean(rip):5.2f}  (att_tau={1/(0.005+ch*0.045)/16:.1f}ms rel_tau={1/(0.001+ch*0.009)/16:.1f}ms) peak={20*np.log10(pk):+6.2f}dB")
print()
print("=== RESONANCE の影響 (BANDS=48, CHAR=1.0) ===")
for r in (0.3,0.5,1.0,2.0,3.0):
    L,R,ET,f0=run2(mod,car,bands=48,reso=r)
    pk=max(np.abs(L).max(),np.abs(R).max())
    print(f"  RESO={r:.1f}  peak={20*np.log10(pk):+6.2f} dBFS")
