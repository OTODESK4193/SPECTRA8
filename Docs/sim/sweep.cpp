// 検波の att/rel 定数を外から与えてビビり指数と応答速度を測る実験ハーネス
#include <cstdio>
#include <cmath>
#include <vector>
#include <array>
static constexpr float SR=16000.0f;
struct SVF{float s1=0,s2=0;};
static std::vector<float> saw(float f,int N){std::vector<float>o(N);float p=0;
 for(int n=0;n<N;++n){o[n]=2*p-1;p+=f/SR;if(p>=1)p-=1;}return o;}
static std::vector<float> vowel(int N){auto x=saw(130.f,N);std::vector<float>y(N,0.f);
 struct F{float fc,q,g;};F fs[3]={{700,8,1.f},{1200,10,.6f},{2600,12,.35f}};
 for(auto&f:fs){float g=std::tan(3.14159265f*f.fc/SR),k=1/f.q,a1=1/(1+g*(g+k));float s1=0,s2=0;
  for(int n=0;n<N;++n){float v=a1*(s1+g*(x[n]-s2));float bp=v,lp=s2+g*v;s1=2*bp-s1;s2=2*lp-s2;y[n]+=f.g*bp/f.q;}}
 float m=0;for(float v:y)m=std::max(m,std::fabs(v));for(auto&v:y)v/=m;return y;}

struct Cfg{ float attBase, attFloorK, relBase, relFloorK; };

// 48バンドの検波器だけを走らせ、全バンドのエンベロープ和を返す
static std::vector<float> runEnv(const std::vector<float>& in,const Cfg& c){
  const int B=48; float f0[48],Q[48],ga[48],a1[48],ac[48],rc[48];
  float mMin=2595*std::log10(1+80.f/700), mMax=2595*std::log10(1+7500.f/700);
  float ed[49];
  for(int i=0;i<B;++i){ float mv=mMin+(mMax-mMin)*i/(B-1); f0[i]=700*(std::pow(10.f,mv/2595)-1); }
  for(int i=1;i<B;++i) ed[i]=std::sqrt(f0[i-1]*f0[i]);
  ed[0]=f0[0]*f0[0]/ed[1]; ed[B]=std::min(7800.f,f0[B-1]*f0[B-1]/ed[B-1]);
  for(int i=0;i<B;++i){ float bw=ed[i+1]-ed[i];
    Q[i]=std::min(24.f,std::max(1.5f,f0[i]/std::max(1.f,bw)));
    ga[i]=std::tan(3.14159265f*f0[i]/SR); a1[i]=1/(1+ga[i]*(ga[i]+1/Q[i]));
    float fr=std::max(60.f,0.35f*f0[i]);
    float aT=std::max(c.attBase, c.attFloorK/fr);
    float rT=std::max(c.relBase, c.relFloorK/fr);
    ac[i]=1-std::exp(-1/(aT*SR)); rc[i]=1-std::exp(-1/(rT*SR)); }
  SVF s[48][2]; float env[48]={0}; std::vector<float> out(in.size());
  for(size_t n=0;n<in.size();++n){ float sum=0;
    for(int i=0;i<B;++i){ auto&A=s[i][0];float v=a1[i]*(A.s1+ga[i]*(in[n]-A.s2));float bp=v,lp=A.s2+ga[i]*v;
      A.s1=2*bp-A.s1;A.s2=2*lp-A.s2;
      auto&Bq=s[i][1];float v2=a1[i]*(Bq.s1+ga[i]*(bp-Bq.s2));float bp2=v2,lp2=Bq.s2+ga[i]*v2;
      Bq.s1=2*bp2-Bq.s1;Bq.s2=2*lp2-Bq.s2;
      float e=std::fabs(bp2/(Q[i]*Q[i]));
      env[i]+= (e>env[i]?ac[i]:rc[i])*(e-env[i]); sum+=env[i]; }
    out[n]=sum; }
  return out;
}
static double buzz(const std::vector<float>& e,double f0,int from){
  double dc=0; int n=0; for(size_t i=from;i<e.size();++i){dc+=e[i];++n;} dc/=n;
  double re=0,im=0,w=2*M_PI*f0/SR;
  for(size_t i=from;i<e.size();++i){re+=(e[i]-dc)*std::cos(w*(i-from));im+=(e[i]-dc)*std::sin(w*(i-from));}
  return 2*std::sqrt(re*re+im*im)/n/(dc+1e-12);
}
int main(){
  const int N=48000; auto v=vowel(N); for(auto&x:v)x*=0.9f;
  // 応答速度テスト: 無音→母音の立ち上がりで包絡が90%に達するまでの時間
  std::vector<float> step(N,0.f); for(int i=N/2;i<N;++i) step[i]=v[i];
  auto resp=[&](const Cfg&c){ auto e=runEnv(step,c);
    double fin=0; for(int i=N-4000;i<N;++i) fin+=e[i]; fin/=4000;
    for(int i=N/2;i<N;++i) if(e[i]>=0.9*fin) return (i-N/2)*1000.0/SR;
    return 999.0; };
  printf("%-34s %-10s %-12s\n","設定 (CHARACTER=1.0相当)","ビビり","立上り90%");
  struct T{const char*n;Cfg c;} ts[]={
    {"旧実装 att1.25ms rel6.25ms",      {1.25e-3f,0.f,6.25e-3f,0.f}},
    {"今回の案 att0.5ms rel8ms+床2.5/f",{0.5e-3f,0.f,8e-3f,2.5f}},
    {"att1.5ms rel8ms+床2.5/f",         {1.5e-3f,0.f,8e-3f,2.5f}},
    {"att3ms rel10ms+床2.5/f",          {3.0e-3f,0.f,10e-3f,2.5f}},
    {"att床0.5/f rel10ms+床2.5/f",      {1.0e-3f,0.5f,10e-3f,2.5f}},
    {"att床1.0/f rel12ms+床3.0/f",      {1.0e-3f,1.0f,12e-3f,3.0f}},
    {"att床1.5/f rel12ms+床3.0/f",      {1.0e-3f,1.5f,12e-3f,3.0f}},
  };
  for(auto&t:ts){ auto e=runEnv(v,t.c);
    printf("%-34s %-10.4f %-12.2f ms\n", t.n, buzz(e,130.0,8000), resp(t.c)); }
  return 0;
}
