#include "DSP/ModMatrix.h"
#include <cstdio>
#include <cmath>
// Saw波の下降エッジ(位相ラップ)を数えて実測周期を求める
static double measureLfoHz(double prepSr, float rateHz, int seconds=40){
  ModMatrix mm; mm.prepare(prepSr);
  ModMatrix::Params p; p.lfo[0].rateHz=rateHz; p.lfo[0].wave=2;   // Saw
  p.slot[0].src=ModMatrix::SrcLfo1; p.slot[0].dst=ModMatrix::DstCharacter; p.slot[0].amt=1.0f;
  const int ticks=(int)(500.0*seconds);           // 16kHz/32 = 500ティック/秒
  int wraps=0; float prev=-1;
  for(int t=0;t<ticks;++t){ mm.processBlock(32,p); float v=mm.get(ModMatrix::DstCharacter);
    if(t>0 && v<prev-0.5f) ++wraps; prev=v; }
  return (double)wraps/seconds;
}
int main(){
  printf("=== LFO レート (Saw波の周期を実測 / 40秒平均) ===\n");
  printf("%-12s %-12s %-12s %s\n","設定Hz","改修後","旧実装(48k)","旧実装(96k)");
  for(float f : {0.5f, 1.0f, 2.0f, 5.0f}){
    printf("%-12.2f %-12.2f %-12.2f %.2f\n", f,
      measureLfoHz(16000.0,f), measureLfoHz(48000.0,f), measureLfoHz(96000.0,f));
  }
  printf("\n=== ENV LOOP の自走 (Attack 0.5s / Decay 0.5s → 周期1.0秒のはず) ===\n");
  { ModMatrix mm; mm.prepare(16000.0);
    ModMatrix::Params p; p.env[0].attack=0.5f; p.env[0].decay=0.5f; p.env[0].loop=true;
    p.slot[0].src=ModMatrix::SrcEnv1; p.slot[0].dst=ModMatrix::DstCharacter;
    p.slot[0].amt=1.0f; p.slot[0].uni=true;
    int peaks=0; float prev=0,prev2=0;
    for(int t=0;t<2000;++t){ mm.processBlock(32,p); float v=mm.get(ModMatrix::DstCharacter);
      if(t>2 && prev>prev2 && prev>v) ++peaks; prev2=prev; prev=v; }
    printf("  4秒間の山の数 = %d → 周期 %.3f 秒 (目標 1.000秒)\n", peaks, peaks?4.0/peaks:0.0); }
  printf("\n=== 非有限値の耐性 (rate=NaN でフリーズしないか) ===\n");
  { ModMatrix mm; mm.prepare(16000.0);
    ModMatrix::Params p; p.lfo[0].rateHz=NAN;
    p.slot[0].src=ModMatrix::SrcLfo1; p.slot[0].dst=ModMatrix::DstCharacter; p.slot[0].amt=1.0f;
    for(int t=0;t<1000;++t) mm.processBlock(32,p);
    float v=mm.get(ModMatrix::DstCharacter);
    printf("  1000ティック完走 / 出力 = %.3f (有限=%s)\n", v, std::isfinite(v)?"YES":"NO"); }
  return 0;
}
