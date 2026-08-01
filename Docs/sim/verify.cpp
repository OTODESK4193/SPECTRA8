#include "DSP/FilterbankVocoder.h"
#include "DSP/LpcVocoder.h"
#include <cstdio>
#include <cmath>
#include <vector>
static constexpr double SR=16000.0;
static std::vector<float> saw(float f,int N){ std::vector<float> o(N); float p=0;
  for(int n=0;n<N;++n){ o[n]=2*p-1; p+=f/(float)SR; if(p>=1)p-=1; } return o; }
static std::vector<float> vowel(int N){
  auto x=saw(130.0f,N); std::vector<float> y(N,0.f);
  struct F{float fc,q,g;}; F fs[3]={{700,8,1.f},{1200,10,.6f},{2600,12,.35f}};
  for(auto&f:fs){ float g=std::tan(3.14159265f*f.fc/(float)SR),k=1/f.q,a1=1/(1+g*(g+k));
    float s1=0,s2=0; for(int n=0;n<N;++n){ float v=a1*(s1+g*(x[n]-s2)); float bp=v,lp=s2+g*v;
      s1=2*bp-s1; s2=2*lp-s2; y[n]+=f.g*bp/f.q; } }
  float m=0; for(float v:y) m=std::max(m,std::fabs(v)); for(auto&v:y) v/=m; return y; }

int main(){
  const int N=32000;
  auto mod=vowel(N); for(auto&v:mod) v*=0.9f;
  auto car=saw(130.0f,N);
  std::array<std::atomic<float>,48> gains, levels;
  for(int i=0;i<48;++i){ gains[i].store(1.0f); levels[i].store(0.0f); }

  printf("=== FilterbankVocoder 実コード検証 (入力 -0.9dBFS 母音 / キャリア Saw130Hz) ===\n");
  printf("%-7s %-10s %-10s %-12s\n","BANDS","peak","rms","0dBFS超え");
  for(int b : {48,32,24,16,8}){
    FilterbankVocoder fb; fb.prepare(SR);
    double pk=0, se=0; int over=0;
    for(int n=0;n<N;++n){ float l=0,r=0;
      fb.processSample(mod[n],car[n],car[n],l,r,b,1.0f,1.0f,0.0f,1.0f,0.6f,gains,levels);
      if(n<4000) continue;                       // 立ち上がりを除外
      double a=std::max(std::fabs(l),std::fabs(r));
      pk=std::max(pk,a); se+=(l*l+r*r)*0.5; if(a>1.0) ++over; }
    int cnt=N-4000;
    printf("%-7d %+8.2fdB %+8.2fdB %9.1f%%\n", b, 20*std::log10(pk+1e-12),
           20*std::log10(std::sqrt(se/cnt)+1e-12), 100.0*over/cnt);
  }

  printf("\n=== 自己ボコード・ユニティ (キャリア=モジュレーター) ===\n");
  for(int b : {48,24,8}){
    FilterbankVocoder fb; fb.prepare(SR);
    double se=0,pk=0;
    for(int n=0;n<N;++n){ float l=0,r=0;
      fb.processSample(mod[n],mod[n],mod[n],l,r,b,1.0f,1.0f,0.0f,1.0f,0.6f,gains,levels);
      if(n<4000) continue; se+=(l*l+r*r)*0.5; pk=std::max(pk,(double)std::max(std::fabs(l),std::fabs(r))); }
    double ie=0; for(int n=4000;n<N;++n) ie+=mod[n]*mod[n];
    printf("BANDS=%2d  in=%+6.2fdB  out=%+6.2fdB  peak=%+6.2fdB\n", b,
      20*std::log10(std::sqrt(ie/(N-4000))), 20*std::log10(std::sqrt(se/(N-4000))), 20*std::log10(pk));
  }

  printf("\n=== BANDS 切替フェード (48→24 の瞬間に不連続が無いか) ===\n");
  { FilterbankVocoder fb; fb.prepare(SR);
    std::vector<float> out(N); float prev=0; double maxJump=0;
    for(int n=0;n<N;++n){ float l=0,r=0; int b=(n<16000)?48:24;
      fb.processSample(mod[n],car[n],car[n],l,r,b,1.0f,1.0f,0.0f,1.0f,0.6f,gains,levels);
      if(n>15000&&n<17000) maxJump=std::max(maxJump,(double)std::fabs(l-prev));
      prev=l; }
    printf("切替前後2000サンプルの最大サンプル間差分 = %.4f  (クリック=大きな不連続)\n", maxJump); }

  printf("\n=== LpcVocoder ORDER 別レベル ===\n");
  for(int ord : {8,10,12,16}){
    LpcVocoder lp; lp.prepare(SR); lp.setFrameRate(50.0f); lp.setQuantBits(0);
    double se=0,pk=0; int cnt=0;
    for(int n=0;n<N;++n){ float l=0,r=0;
      lp.processSample(mod[n],car[n],car[n],l,r,ord,false,0.998f,0.0f,1.0f);
      if(n<8000) continue; se+=(l*l+r*r)*0.5; pk=std::max(pk,(double)std::max(std::fabs(l),std::fabs(r))); ++cnt; }
    printf("Order %2d : rms=%+6.2fdB  peak=%+6.2fdB\n", ord,
           20*std::log10(std::sqrt(se/cnt)+1e-12), 20*std::log10(pk+1e-12));
  }
  return 0;
}
