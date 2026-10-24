// Independent reimplementation of published collision rules, 2026-10-24
#include <array>
#include <vector>
#include <map>
#include <tuple>
#include <algorithm>
#include <iostream>
#include <random>
#include <cstdint>
#include <cmath>
#include <cassert>
using namespace std;
using S = array<uint8_t,27>;
using F = array<int,6>;
using M = array<int,9>;
static const array<array<int,3>,27> v=[] {array<array<int,3>,27> a{};int k=0;for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)for(int z=-1;z<=1;z++)a[k++]={x,y,z};return a;}();
static const array<int,6> fc={22,4,16,10,14,12};
static const array<array<int,4>,3> eq={{{1,7,19,25},{3,5,21,23},{9,11,15,17}}};
int mod(int x,int l){return (x%l+l)%l;}
int code6(const F&a){int n=0,p=1;for(int x:a){n+=x*p;p*=3;}return n;}
F dec6(int n){F d{};for(auto& x:d){x=n%3;n/=3;}return d;}
int code4(const array<int,4>&a){return a[0]+3*a[1]+9*a[2]+27*a[3];}
array<int,4> dec4(int n){array<int,4>d{};for(int&x:d){x=n%3;n/=3;}return d;}
vector<M> makeoh(){vector<M> ret;array<int,3>p={0,1,2};do{for(int a:{-1,1})for(int b:{-1,1})for(int c:{-1,1}){M m{};array<int,3> s={a,b,c};for(int i=0;i<3;i++)m[3*i+p[i]]=s[i];ret.push_back(m);}}while(next_permutation(p.begin(),p.end()));return ret;}
array<int,3> rxyz(const array<int,3>&a,const M&m){return {m[0]*a[0]+m[1]*a[1]+m[2]*a[2],m[3]*a[0]+m[4]*a[1]+m[5]*a[2],m[6]*a[0]+m[7]*a[1]+m[8]*a[2]};}
int vidx(const array<int,3>&w){for(int i=0;i<27;i++)if(w==v[i])return i;abort();}
S rotcell(const S&a,const M&m){S o{};for(int k=0;k<27;k++)o[vidx(rxyz(v[k],m))]=a[k];return o;}
F rotate6(const F&a,const M&m){F o{};for(int i=0;i<6;i++){int k=vidx(rxyz(v[fc[i]],m));auto it=find(fc.begin(),fc.end(),k);assert(it!=fc.end());o[int(it-fc.begin())]=a[i];}return o;}
int cmass(const S&s){int m=0;for(uint8_t n:s)m+=n;return m;}
array<int,5> vals(const S&s){array<int,5>a={};for(int i=0;i<27;i++){a[0]+=s[i];for(int j=0;j<3;j++)a[j+1]+=s[i]*v[i][j];int e=0;for(int j=0;j<3;j++)e+=v[i][j]*v[i][j];a[4]+=s[i]*e;}return a;}
array<int,4> shells(const S&s){array<int,4>a{};for(int i=0;i<27;i++){int e=0;for(int j=0;j<3;j++)e+=v[i][j]*v[i][j];a[e]+=s[i];}return a;}
array<int,729> facelut(){array<int,729> a{};for(int i=0;i<729;i++)a[i]=i;const array<array<int,2>,7> seeds={{{8,360},{17,49},{23,343},{50,658},{53,341},{80,688},{107,427}}};for(auto [x,y]:seeds)for(const M&m:makeoh()){int rx=code6(rotate6(dec6(x),m)),ry=code6(rotate6(dec6(y),m));if(a[rx]!=rx&&a[rx]!=ry){cerr<<"ambiguous orbit map\n";abort();}a[rx]=ry;a[ry]=rx;}return a;}
array<array<int,81>,3> edgelut(){array<array<int,81>,3> lut{};for(int gi=0;gi<3;gi++){auto& t=lut[gi];for(int i=0;i<81;i++)t[i]=i;map<array<int,5>,vector<int>> cl;for(int i=0;i<81;i++){auto d=dec4(i);array<int,5> key{};for(int j=0;j<4;j++){int k=eq[gi][j];key[0]+=d[j];for(int z=0;z<3;z++)key[z+1]+=d[j]*v[k][z];int e=0;for(int z=0;z<3;z++)e+=v[k][z]*v[k][z];key[4]+=d[j]*e;}cl[key].push_back(i);}for(const auto&[key,codes]:cl){vector<int> c;for(int n:codes)if(n!=40)c.push_back(n);for(int j=0;j+1<int(c.size());j+=2){t[c[j]]=c[j+1];t[c[j+1]]=c[j];}}}return lut;}
S update(S s,const array<int,729>&f,const array<array<int,81>,3>&e){for(int gi=0;gi<3;gi++){array<int,4>d{};for(int j=0;j<4;j++)d[j]=s[eq[gi][j]];int n=e[gi][code4(d)];auto out=dec4(n);for(int j=0;j<4;j++)s[eq[gi][j]]=out[j];}F in{};for(int j=0;j<6;j++)in[j]=s[fc[j]];auto o=dec6(f[code6(in)]);for(int j=0;j<6;j++)s[fc[j]]=uint8_t(o[j]);return s;}
int idx(int x,int y,int z,int l){return (x*l+y)*l+z;}
using State=vector<S>;
State rotgrid(const State&a,int l,const M&m){State b(a.size());for(int x=0;x<l;x++)for(int y=0;y<l;y++)for(int z=0;z<l;z++){array<int,3> r=rxyz({x,y,z},m);b[idx(mod(r[0],l),mod(r[1],l),mod(r[2],l),l)]=rotcell(a[idx(x,y,z,l)],m);}return b;}
State step(const State&a,int l,const array<int,729>&f,const array<array<int,81>,3>&e){State b(a.size());for(int x=0;x<l;x++)for(int y=0;y<l;y++)for(int z=0;z<l;z++){S s=update(a[idx(x,y,z,l)],f,e);for(int k=0;k<27;k++)b[idx(mod(x+v[k][0],l),mod(y+v[k][1],l),mod(z+v[k][2],l),l)][k]=s[k];}return b;}

#include <chrono>
#include <fstream>
struct QuadCross { int c,f,e1,e2; };
struct CrossData { int rest=13;vector<int> edges;map<int,array<int,2>> ef;vector<QuadCross> corner; };
CrossData build_cross(){
 CrossData d;
 for(int k=0;k<27;k++)if(v[k][0]*v[k][0]+v[k][1]*v[k][1]+v[k][2]*v[k][2]==2)d.edges.push_back(k);
 auto E=[](int k){return v[k][0]*v[k][0]+v[k][1]*v[k][1]+v[k][2]*v[k][2];};
 for(int e:d.edges)for(int f1=0;f1<27;f1++)if(E(f1)==1){
  for(int f2=f1+1;f2<27;f2++)if(E(f2)==1){
   if(v[f1][0]+v[f2][0]==v[e][0]&&v[f1][1]+v[f2][1]==v[e][1]&&v[f1][2]+v[f2][2]==v[e][2]){d.ef[e]={f1,f2};break;}
  }
 }
 for(int c=0;c<27;c++)if(E(c)==3)for(int f=0;f<27;f++)if(E(f)==1){
  for(int e1=0;e1<27;e1++)if(E(e1)==2)for(int e2=e1+1;e2<27;e2++)if(E(e2)==2){
   if(v[e1][0]+v[e2][0]==v[c][0]+v[f][0]&&v[e1][1]+v[e2][1]==v[c][1]+v[f][1]&&v[e1][2]+v[e2][2]==v[c][2]+v[f][2])d.corner.push_back({c,f,e1,e2});
  }
 }
 return d;
}
void rest_edge_face(S &s, const CrossData &d){
 int r=s[d.rest];
 if(r==2){
  int ce=-1,n=0;
  for(int e:d.edges){auto p=d.ef.at(e);if(s[e]==2&&s[p[0]]==0&&s[p[1]]==0){ce=e;n++;}}
  if(n==1){auto p=d.ef.at(ce);S sc=s;sc[d.rest]=1;sc[ce]=1;sc[p[0]]=1;sc[p[1]]=1;
   int bc=0,be=-1;for(int eb:d.edges){auto pb=d.ef.at(eb);if(sc[eb]==1&&sc[pb[0]]==1&&sc[pb[1]]==1){be=eb;bc++;}}
   if(bc==1&&be==ce){s=sc;}
  }
 }else if(r==1){
  int be=-1,n=0;
  for(int eb:d.edges){auto p=d.ef.at(eb);if(s[eb]==1&&s[p[0]]==1&&s[p[1]]==1){be=eb;n++;}}
  if(n==1){auto p=d.ef.at(be);S sc=s;sc[d.rest]=2;sc[be]=2;sc[p[0]]=0;sc[p[1]]=0;
   int fc=0,fe=-1;for(int e:d.edges){auto pp=d.ef.at(e);if(sc[e]==2&&sc[pp[0]]==0&&sc[pp[1]]==0){fe=e;fc++;}}
   if(fc==1&&fe==be)s=sc;
  }
 }
}
void corner_face_edge(S &s,const CrossData &d){
 int cf=-1,nf=0;
 for(size_t i=0;i<d.corner.size();i++){auto q=d.corner[i];if(s[q.c]==2&&s[q.f]==2&&s[q.e1]==0&&s[q.e2]==0){cf=int(i);nf++;}}
 if(nf==1){auto q=d.corner[cf];S sc=s;sc[q.c]=sc[q.f]=sc[q.e1]=sc[q.e2]=1;
  int nb=0,bi=-1;for(size_t i=0;i<d.corner.size();i++){auto p=d.corner[i];if(sc[p.c]==1&&sc[p.f]==1&&sc[p.e1]==1&&sc[p.e2]==1){bi=int(i);nb++;}}
  if(nb==1&&bi==cf){s=sc;return;}
 }
 int cb=-1,nb=0;
 for(size_t i=0;i<d.corner.size();i++){auto q=d.corner[i];if(s[q.c]==1&&s[q.f]==1&&s[q.e1]==1&&s[q.e2]==1){cb=int(i);nb++;}}
 if(nb==1){auto q=d.corner[cb];S sc=s;sc[q.c]=sc[q.f]=2;sc[q.e1]=sc[q.e2]=0;
  int nf2=0,fi=-1;for(size_t i=0;i<d.corner.size();i++){auto p=d.corner[i];if(sc[p.c]==2&&sc[p.f]==2&&sc[p.e1]==0&&sc[p.e2]==0){fi=int(i);nf2++;}}
  if(nf2==1&&fi==cb){s=sc;return;}
 }
}
S full(S s,const array<int,729>&face,const array<array<int,81>,3>&edge,const CrossData &d){s=update(s,face,edge);rest_edge_face(s,d);corner_face_edge(s,d);return s;}
S backward(S s,const array<int,729>&face,const array<array<int,81>,3>&edge,const CrossData &d){corner_face_edge(s,d);rest_edge_face(s,d);return update(s,face,edge);}
State fullstep(const State& st,int L,const array<int,729>&face,const array<array<int,81>,3>&edge,const CrossData &d){State out(st.size());for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){S s=full(st[idx(x,y,z,L)],face,edge,d);for(int k=0;k<27;k++)out[idx(mod(x+v[k][0],L),mod(y+v[k][1],L),mod(z+v[k][2],L),L)][k]=s[k];}return out;}

#include <iomanip>
#include <fstream>
#include <numeric>
struct Observe {double kin, enstrophy, coherent, entropy, maxv; int minrho,maxrho;long long N,E,Px,Py,Pz;};
Observe obs(const State &s,int L){
 const int volume=L*L*L;const double pi=std::acos(-1.0);const double kv=2*pi/L;
 std::vector<std::array<double,3>> u(s.size());
 long long mass=0,energy=0,px=0,py=0,pz=0;
 int mn=100,mx=-1;std::array<long long,3> trits{};
 double sumkin=0;
 for(int a=0;a<volume;a++){
   auto inv=vals(s[a]);mass+=inv[0];px+=inv[1];py+=inv[2];pz+=inv[3];energy+=inv[4];mn=std::min(mn,inv[0]);mx=std::max(mx,inv[0]);
   for(const auto n:s[a])trits[n]++;
   for(int j=0;j<3;j++)u[a][j]=(inv[0]? double(inv[j+1])/inv[0]:0.0);
   if(inv[0])sumkin+=(double(inv[1])*inv[1]+double(inv[2])*inv[2]+double(inv[3])*inv[3])/(2.0*inv[0]);
 }
 auto U=[&](int x,int y,int z,int k){return u[idx(mod(x,L),mod(y,L),mod(z,L),L)][k];};
 double ens=0,maxv=0,nproj=0,dproj=0;
 for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){
   double wx=(U(x,y+1,z,2)-U(x,y-1,z,2)-U(x,y,z+1,1)+U(x,y,z-1,1))*0.5;
   double wy=(U(x,y,z+1,0)-U(x,y,z-1,0)-U(x+1,y,z,2)+U(x-1,y,z,2))*0.5;
   double wz=(U(x+1,y,z,1)-U(x-1,y,z,1)-U(x,y+1,z,0)+U(x,y-1,z,0))*0.5;
   double curl2=wx*wx+wy*wy+wz*wz;ens+=curl2*0.5;maxv=std::max(maxv,std::sqrt(curl2));
   double tgx=std::sin(kv*(x+.5))*std::cos(kv*(y+.5))*std::cos(kv*(z+.5));
   double tgy=-std::cos(kv*(x+.5))*std::sin(kv*(y+.5))*std::cos(kv*(z+.5));
   nproj+=U(x,y,z,0)*tgx+U(x,y,z,1)*tgy;dproj+=tgx*tgx+tgy*tgy;
 }
 double entropy=0;for(auto c:trits)if(c){double p=double(c)/(volume*27);entropy-=p*std::log(p);}
 return {sumkin/volume,ens/volume,nproj/dproj,entropy,maxv,mn,mx,mass,energy,px,py,pz};
}
int main(){
 auto f=facelut();auto e=edgelut();auto d=build_cross();const int L=24,volume=L*L*L;const double pi=std::acos(-1.0),kv=2*pi/L,U0=0.20;
 State st(volume);for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){
  S q{};q.fill(1);
  double tx=U0*std::sin(kv*(x+.5))*std::cos(kv*(y+.5))*std::cos(kv*(z+.5));
  double ty=-U0*std::cos(kv*(x+.5))*std::sin(kv*(y+.5))*std::cos(kv*(z+.5));
  auto quant=[](double t){int sign=(t>0)-(t<0);int mag=int(std::floor(std::abs(27*t/2)+0.5))*2;return sign*std::min(mag,4);};
  auto set=[&](int val,int fp,int fm,int ep1,int em1,int ep2,int em2){int df=(std::abs(val)==2)?(val>0?1:-1):0;int de=(std::abs(val)==4)?(val>0?1:-1):0;q[fp]=uint8_t(1+df);q[fm]=uint8_t(1-df);q[ep1]=q[ep2]=uint8_t(1+de);q[em1]=q[em2]=uint8_t(1-de);};
  set(quant(tx),22,4,23,3,21,5);set(quant(ty),16,10,17,9,15,11);st[idx(x,y,z,L)]=q;
 }
 const auto start=obs(st,L);bool pass=true;
 std::ofstream o("independent_arm5_results.csv");o<<"tick,mass,micro_energy,px,py,pz,kinetic,enstrophy,max_vorticity,coherent_tgv,entropy_trit,rho_min,rho_max\n";
 for(int t=0;t<=48;t++){
 auto a=obs(st,L);o<<t<<","<<a.N<<","<<a.E<<","<<a.Px<<","<<a.Py<<","<<a.Pz<<","<<std::setprecision(14)<<a.kin<<","<<a.enstrophy<<","<<a.maxv<<","<<a.coherent<<","<<a.entropy<<","<<a.minrho<<","<<a.maxrho<<"\n";
 pass &= (a.N==start.N&&a.E==start.E&&a.Px==0&&a.Py==0&&a.Pz==0);
 if(t==0||t==1||t==3||t==6||t==12||t==24||t==36||t==48)std::cout<<"ARM5 t="<<t<<" kinetic="<<std::setprecision(12)<<a.kin<<" omega="<<a.enstrophy<<" coherent="<<a.coherent<<" entropy="<<a.entropy<<" max_v="<<a.maxv<<" rho="<<a.minrho<<".."<<a.maxrho<<"\n";
 if(t<48)st=fullstep(st,L,f,e,d);
 }
 std::cout<<"CHECK total_N="<<start.N<<" doubled_micro_E="<<start.E<<" all_ticks_exact_conservation="<<pass<<"\n";
 return pass?0:1;
}
