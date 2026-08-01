import numpy as np
SR=16000.0; NB=48
def layout(bands):
    fMin,fMax=80.0,7500.0
    mMin=2595*np.log10(1+fMin/700); mMax=2595*np.log10(1+fMax/700)
    m=mMin+(mMax-mMin)*np.arange(bands)/(bands-1)
    f0=700*(10**(m/2595)-1)
    edges=np.zeros(bands+1)
    for i in range(1,bands): edges[i]=np.sqrt(f0[i-1]*f0[i])
    edges[0]=f0[0]*f0[0]/edges[1]
    edges[bands]=min(7800.0, f0[bands-1]**2/edges[bands-1])
    bw=edges[1:]-edges[:-1]
    Q=np.clip(f0/np.maximum(1.0,bw),1.5,24.0)
    return f0,Q

def coeffs(fc,Q):
    g=np.tan(np.pi*fc/SR); k=1.0/Q; a1=1.0/(1.0+g*(g+k)); return g,k,a1

def run(mod,car,bands=48,character=1.0,reso=1.0,stretch=1.0,shift=0.0):
    f0,Qb=layout(bands)
    Q=np.clip(Qb*reso,1.0,40.0)
    ga,ka,a1a=coeffs(f0,Q)
    f0s=np.clip(f0*stretch*2**(shift/12),50,7800)
    gs,ks,a1s=coeffs(f0s,Q)
    N=len(mod)
    s1a=np.zeros(bands);s2a=np.zeros(bands);s1b=np.zeros(bands);s2b=np.zeros(bands)
    p1=np.zeros(bands);p2=np.zeros(bands);q1=np.zeros(bands);q2=np.zeros(bands)
    env=np.zeros(bands)
    att=0.005+character*0.045; rel=0.001+character*0.009
    invQ2=1.0/(Q*Q)
    out=np.zeros(N); envtrace=np.zeros((N,bands))
    bandComp=np.sqrt(48.0/bands); mk=32.36*bandComp
    for n in range(N):
        x=mod[n]
        v=a1a*(s1a+ga*(x-s2a)); ybp=v; ylp=s2a+ga*v
        s1a=2*ybp-s1a; s2a=2*ylp-s2a
        v2=a1a*(s1b+ga*(ybp-s2b)); ybp2=v2; ylp2=s2b+ga*v2
        s1b=2*ybp2-s1b; s2b=2*ylp2-s2b
        an=ybp2/(Q*Q)
        e=np.abs(an)
        c=np.where(e>env,att,rel); env+=c*(e-env)
        envtrace[n]=env
        cc=car[n]
        w=a1s*(p1+gs*(cc-p2)); wbp=w; wlp=p2+gs*w
        p1=2*wbp-p1; p2=2*wlp-p2
        w2=a1s*(q1+gs*(wbp-q2)); wbp2=w2; wlp2=q2+gs*w2
        q1=2*wbp2-q1; q2=2*wlp2-q2
        co=wbp2*invQ2
        out[n]=np.sum(co*env)*mk   # panL~cos(45)=0.707 omitted -> see note
    return out,envtrace,f0

def saw(f,N):
    ph=(np.arange(N)*f/SR)%1.0
    return 2*ph-1

def voice(N,f0=130.0):
    # synthetic vowel: saw through 3 formant resonators
    x=saw(f0,N)
    y=np.zeros(N)
    for fc,q,g in [(700,8,1.0),(1200,10,0.6),(2600,12,0.35)]:
        gg=np.tan(np.pi*fc/SR); kk=1/q; a1=1/(1+gg*(gg+kk))
        s1=s2=0.0; o=np.zeros(N)
        for n in range(N):
            v=a1*(s1+gg*(x[n]-s2)); bp=v; lp=s2+gg*v
            s1=2*bp-s1; s2=2*lp-s2; o[n]=bp
        y+=g*o/q
    y/=np.max(np.abs(y)); return y

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
