#include "FBHDR"
#include <cstdio>
#include <cmath>
#include <vector>
static constexpr double SR=16000.0;
static std::vector<float> saw(float f,int N){std::vector<float>o(N);float p=0;
 for(int n=0;n<N;++n){o[n]=2*p-1;p+=f/(float)SR;if(p>=1)p-=1;}return o;}
static std::vector<float> vowel(int N){auto x=saw(130.f,N);std::vector<float>y(N,0.f);
 struct F{float fc,q,g;};F fs[3]={{700,8,1.f},{1200,10,.6f},{2600,12,.35f}};
 for(auto&f:fs){float g=std::tan(3.14159265f*f.fc/(float)SR),k=1/f.q,a1=1/(1+g*(g+k));float s1=0,s2=0;
  for(int n=0;n<N;++n){float v=a1*(s1+g*(x[n]-s2));float bp=v,lp=s2+g*v;s1=2*bp-s1;s2=2*lp-s2;y[n]+=f.g*bp/f.q;}}
 float m=0;for(float v:y)m=std::max(m,std::fabs(v));for(auto&v:y)v/=m;return y;}
static double buzz(const std::vector<float>&e,double f0,int from){
  double dc=0;int n=0;for(size_t i=from;i<e.size();++i){dc+=e[i];++n;}dc/=n;
  double re=0,im=0,w=2*M_PI*f0/SR;
  for(size_t i=from;i<e.size();++i){re+=(e[i]-dc)*std::cos(w*(i-from));im+=(e[i]-dc)*std::sin(w*(i-from));}
  return 2*std::sqrt(re*re+im*im)/n/(dc+1e-12);}
int main(){
  const int N=48000; auto v=vowel(N); for(auto&x:v)x*=0.9f;
  std::array<std::atomic<float>,48> lv;
  printf("CHARACTER  検波リップル(=ビビり音の直接指標)\n");
  for(float ch : {0.0f,0.25f,0.5f,0.75f,1.0f}){
    FilterbankVocoder fb; fb.prepare(SR);
    for(int i=0;i<48;++i) lv[i].store(0.f);
    std::vector<float> e(N);
    for(int n=0;n<N;++n){ fb.analyzeForMeter(v[n],48,ch,1.0f,lv);
      float s=0; for(int i=0;i<48;++i) s+=lv[i].load(); e[n]=s; }
    printf("  %.2f      %.4f\n", ch, buzz(e,130.0,8000));
  }
  return 0;
}
