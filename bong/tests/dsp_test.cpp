#include "../Source/Engine.h"
#include <cstdio>
#include <chrono>
using namespace bong;
Params base(){
  Params p{};
  p.g_depth=.7f;p.g_rate=.37f;p.g_smooth=.74f;p.r_len=.11f;p.r_count=.48f;p.r_tone=.85f;p.v_len=.22f;p.v_chance=.59f;p.v_fade=.96f;
  p.s_pieces=.33f;p.s_blend=.7f;p.s_chance=.07f;p.f_cut=.44f;p.f_res=.81f;p.f_depth=.18f;p.b_speed=.55f;p.b_curve=.92f;p.b_tone=.29f;
  float a[6]={.8f,.5f,.6f,.9f,.4f,.55f}; for(int i=0;i<6;i++)p.amt[i]=a[i];
  p.rhythmMix=.64f;p.swing=.2f;p.sync=1;p.seqLen=16;p.seqDir=0;
  uint16_t pt[6]={0x5555,0xC0C0,0x1010,0x8000,0x0C0C,0x0101}; for(int i=0;i<6;i++)p.pattern[i]=pt[i];
  p.k_size=.1f;p.k_dens=.51f;p.k_spread=.92f;p.z_hold=.33f;p.z_fade=.74f;p.z_tone=.15f;p.y_size=.56f;p.y_tail=.97f;p.y_color=.38f;
  p.h_oct=.79f;p.h_amt=.2f;p.h_glit=.61f;p.t_drift=.02f;p.t_cut=.43f;p.t_speed=.84f;p.w_wow=.25f;p.w_flut=.66f;p.w_hiss=.07f;
  for(int i=0;i<6;i++)p.atmosOn[i]=false; p.atmosOn[2]=true; p.atmosMix=.55f;p.freeze=false;p.outputGain=1;p.seed=0x4F2A; return p;}
struct R{float peak=0,rms=0;bool bad=false;};
R run(Params p,double sr,int secs,float amp,bool freezeMid=false,double bpm=124){
  Rhythm rh; Atmos at; rh.prepare(sr); at.prepare(sr);
  int B=32,n=(int)(sr*secs); std::vector<float>L(B),Rr(B); R r; double acc=0;long cnt=0; double ppq=0;
  for(int s=0;s<n;s+=B){
    for(int j=0;j<B;j++){double t=(s+j)/sr; float env=std::exp(-std::fmod(t,60.0/bpm)*8); float x=amp*(0.5f*std::sin(2*kPi*110*t)*env+0.3f*std::sin(2*kPi*880*t)); L[j]=x;Rr[j]=x*0.8f;}
    if(freezeMid) p.freeze = (s>n/3 && s<2*n/3);
    Clock c{ppq,bpm/60.0/sr,bpm}; float* ch[2]={L.data(),Rr.data()};
    rh.process(ch,2,B,p,c); at.process(ch,2,B,p); ppq+=bpm/60.0/sr*B;
    for(int j=0;j<B;j++)for(float v:{L[j],Rr[j]}){if(!std::isfinite(v))r.bad=true;r.peak=std::max(r.peak,std::fabs(v));acc+=v*v;cnt++;}
  }
  r.rms=std::sqrt(acc/cnt);return r;}
void rep(const char*nm,Params p,bool fz=false){ for(double sr:{44100.0,96000.0}){R r=run(p,sr,6,0.7f,fz); printf("%-26s sr=%6.0f peak=%7.2f dB rms=%7.2f dB %s\n",nm,sr,20*log10(r.peak+1e-9),20*log10(r.rms+1e-9),r.bad?"NAN!!":"");}}
int main(){
  Params p=base(); rep("default",p);
  {Params q=p; for(int i=0;i<6;i++){q.pattern[i]=0xFFFF;q.amt[i]=1;} q.rhythmMix=1; rep("all rhythm steps max",q);}
  {Params q=p; q.pattern[5]=0xFFFF;q.amt[5]=1;q.b_speed=0;q.rhythmMix=1; rep("brems every step",q);}
  {Params q=p; q.pattern[2]=0xFFFF;q.amt[2]=1;q.v_chance=1;q.v_len=1;q.rhythmMix=1; rep("baglæns every step",q);}
  {Params q=p; q.pattern[3]=0xFFFF;q.amt[3]=1;q.s_chance=1;q.rhythmMix=1;q.sync=0; rep("skær 1/8",q);}
  {Params q=p; q.sync=2;q.swing=1;q.seqDir=3;q.seqLen=5; rep("1/32 swing random len5",q);}
  {Params q=p; for(int i=0;i<6;i++)q.atmosOn[i]=true; q.atmosMix=1; rep("all atmos on",q,true);}
  {Params q=p; for(int i=0;i<6;i++)q.atmosOn[i]=true; q.atmosMix=1;q.h_amt=1;q.y_tail=1;q.k_dens=1;q.k_spread=1;q.w_wow=1;q.w_flut=1;q.w_hiss=1;q.t_drift=1; rep("all atmos MAX",q,true);}
  {Params q=p; for(int i=0;i<6;i++){q.pattern[i]=0xFFFF;q.amt[i]=1;q.atmosOn[i]=true;} q.rhythmMix=1;q.atmosMix=1;q.h_amt=1;q.k_dens=1; rep("EVERYTHING",q,true);}
  // bypass check: all off
  {Params q=p; for(int i=0;i<6;i++){q.pattern[i]=0;q.atmosOn[i]=false;} Rhythm rh;Atmos at;rh.prepare(48000);at.prepare(48000);
   float L[64],Rr[64],ref[64]; for(int i=0;i<64;i++){L[i]=Rr[i]=ref[i]=0.5f*std::sin(i*.1f);} Clock c{0,124/60.0/48000,124}; float*ch[2]={L,Rr}; rh.process(ch,2,64,q,c); at.process(ch,2,64,q);
   float md=0; for(int i=0;i<64;i++) md=std::max(md,std::fabs(L[i]-ref[i])); printf("all-off deviation: %g\n",md);}
  // silence tail
  {Params q=p; for(int i=0;i<6;i++)q.atmosOn[i]=true; q.atmosMix=1; R r=run(q,48000,4,0.0f); printf("silence in, atmos on: peak %.1f dB (hiss expected)\n",20*log10(r.peak+1e-9));}
  auto t0=std::chrono::steady_clock::now(); {Params q=p; for(int i=0;i<6;i++){q.pattern[i]=0xFFFF;q.atmosOn[i]=true;} q.k_dens=1; run(q,48000,20,0.5f,true);} 
  double dt=std::chrono::duration<double>(std::chrono::steady_clock::now()-t0).count(); printf("CPU worst case: %.1f%% of realtime\n",dt/20*100);
}
int main_iso(){
  const char* nm[6]={"KORN","FRYS","SKY","SHIMMER","TAGE","BAND"};
  for(int e=0;e<6;e++){ Params q=base(); for(int i=0;i<6;i++){q.pattern[i]=0;q.atmosOn[i]=(i==e);} if(e==3)q.atmosOn[2]=false; q.atmosMix=1;
    q.k_dens=1;q.k_spread=1;q.k_size=1;q.h_amt=1;q.y_tail=1;q.z_hold=1;q.w_wow=1;q.w_flut=1;q.w_hiss=1;q.t_drift=1;
    R r=run(q,48000,8,0.7f,true); printf("%-8s max: peak %6.2f dB rms %6.2f dB\n",nm[e],20*log10(r.peak),20*log10(r.rms)); }
  { Params q=base(); for(int i=0;i<6;i++)q.pattern[i]=0; q.atmosOn[2]=true;q.atmosOn[3]=true;q.h_amt=1;q.y_tail=1;q.atmosMix=1; R r=run(q,48000,8,0.7f); printf("SKY+SHIM max: peak %6.2f rms %6.2f\n",20*log10(r.peak),20*log10(r.rms)); }
  { Params q=base(); for(int i=0;i<6;i++)q.pattern[i]=0; q.atmosMix=0; R r=run(q,48000,8,0.7f); printf("dry ref: peak %6.2f rms %6.2f\n",20*log10(r.peak),20*log10(r.rms)); }
}
