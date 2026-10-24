#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <tuple>
#include <vector>
using std::array; using std::vector;
using U = std::uint8_t;
struct Vel { int x,y,z,e; };
struct Gate { int a,b,c,d;array<U,81> lut;};
static constexpr int K=27;
static array<Vel,K> velocities(){array<Vel,K> v{};int k=0;for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++)for(int z=-1;z<=1;z++)v[k++]={x,y,z,x*x+y*y+z*z};return v;}
static array<int,4> decode(int code){array<int,4> a{};for(int j=0;j<4;j++){a[j]=code%3;code/=3;}return a;}
static int encode(int a,int b,int c,int d){return a+3*b+9*c+27*d;}
static array<long long,5> inv(const array<int,4>& d,const array<int,4>& q,const array<Vel,K>& v){array<long long,5> s{};for(int j=0;j<4;j++){int n=d[j];auto t=v[q[j]];s[0]+=n;s[1]+=n*t.x;s[2]+=n*t.y;s[3]+=n*t.z;s[4]+=n*t.e;}return s;}
static vector<Gate> gen(const array<Vel,K>& v,int variant){using Key=std::tuple<int,int,int,int>;std::map<Key,vector<array<int,2>>> pairmap;for(int i=0;i<K;i++)for(int j=i+1;j<K;j++){auto a=v[i],b=v[j];pairmap[{a.x+b.x,a.y+b.y,a.z+b.z,a.e+b.e}].push_back({i,j});}
 std::set<array<int,4>> qq;for(const auto& ent:pairmap){const auto& pairs=ent.second;for(size_t i=0;i<pairs.size();i++)for(size_t j=i+1;j<pairs.size();j++){auto a=pairs[i],b=pairs[j];array<int,4> q{a[0],a[1],b[0],b[1]};std::sort(q.begin(),q.end());if(std::adjacent_find(q.begin(),q.end())==q.end())qq.insert(q);}}
 vector<Gate> res;size_t qi=0;for(const auto& q:qq){Gate g{q[0],q[1],q[2],q[3],{}};std::map<array<long long,5>,vector<int>> cls;for(int mask=0;mask<81;mask++){g.lut[mask]=static_cast<U>(mask);cls[inv(decode(mask),q,v)].push_back(mask);}for(const auto& ent:cls){vector<int> cs;for(int code:ent.second)if(code!=40)cs.push_back(code);bool perform=(variant==0)||((ent.first[0]+static_cast<long long>(qi))%2==0);if(perform)for(size_t j=0;j+1<cs.size();j+=2){g.lut[cs[j]]=static_cast<U>(cs[j+1]);g.lut[cs[j+1]]=static_cast<U>(cs[j]);}}bool active=false;for(int i=0;i<81;i++)if(g.lut[i]!=i)active=true;if(active)res.push_back(g);qi++;}return res;}
static long long checksum(const vector<U>& s,const array<Vel,K>& v,int component){long long sum=0;for(size_t i=0;i<s.size();i++){const auto& vel=v[i%27];int val=component==0?1:component==1?vel.x:component==2?vel.y:component==3?vel.z:vel.e;sum+=static_cast<int>(s[i])*val;}return sum;}
static int cellid(int x,int y,int z,int L){return (x*L+y)*L+z;}
static vector<U> ground(int L){return vector<U>(L*L*L*K,1);}
static vector<U> tg(int L,double U0,const array<Vel,K>& v){vector<U> s=ground(L);const double pi=std::acos(-1.0), k=2*pi/L;for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){double tx=U0*std::sin(k*(x+.5))*std::cos(k*(y+.5))*std::cos(k*(z+.5));double ty=-U0*std::cos(k*(x+.5))*std::sin(k*(y+.5))*std::cos(k*(z+.5));auto quant=[](double t){return std::clamp(static_cast<int>(2.0*std::round(27.0*t/2.0)),-4,4);};int nx=quant(tx),ny=quant(ty);int c=cellid(x,y,z,L)*K;auto assign=[&](int val,int p,int m,int p1,int m1,int p2,int m2){int df=val==2?1:val==-2?-1:0;int de=val==4?1:val==-4?-1:0;s[c+p]=static_cast<U>(1+df);s[c+m]=static_cast<U>(1-df);s[c+p1]=static_cast<U>(1+de);s[c+m1]=static_cast<U>(1-de);s[c+p2]=static_cast<U>(1+de);s[c+m2]=static_cast<U>(1-de);};assign(nx,22,4,23,3,21,5);assign(ny,16,10,17,9,15,11);} (void)v;return s;}
static void tick(vector<U>& state,vector<U>& next,const vector<Gate>& g,const array<Vel,K>& v,int L,int t,bool backwards){int V=L*L*L;if(!backwards){for(int cell=0;cell<V;cell++)for(int j=0;j<32;j++){size_t which=(static_cast<size_t>(t)*32+static_cast<size_t>(cell)*17+static_cast<size_t>(j)*37)%g.size();auto col=g[which];int b=cell*K;auto d=decode(col.lut[encode(state[b+col.a],state[b+col.b],state[b+col.c],state[b+col.d])]);state[b+col.a]=static_cast<U>(d[0]);state[b+col.b]=static_cast<U>(d[1]);state[b+col.c]=static_cast<U>(d[2]);state[b+col.d]=static_cast<U>(d[3]);}}
 for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++)for(int k=0;k<K;k++){int nx=(x+(backwards?-v[k].x:v[k].x)+L)%L,ny=(y+(backwards?-v[k].y:v[k].y)+L)%L,nz=(z+(backwards?-v[k].z:v[k].z)+L)%L;next[cellid(nx,ny,nz,L)*K+k]=state[cellid(x,y,z,L)*K+k];}
 state.swap(next);
 if(backwards){for(int cell=0;cell<V;cell++)for(int j=31;j>=0;j--){size_t which=(static_cast<size_t>(t)*32+static_cast<size_t>(cell)*17+static_cast<size_t>(j)*37)%g.size();auto col=g[which];int b=cell*K;auto d=decode(col.lut[encode(state[b+col.a],state[b+col.b],state[b+col.c],state[b+col.d])]);state[b+col.a]=static_cast<U>(d[0]);state[b+col.b]=static_cast<U>(d[1]);state[b+col.c]=static_cast<U>(d[2]);state[b+col.d]=static_cast<U>(d[3]);}}}
struct Stat{double energy,kin,enstrophy,maxv,densityMin,densityMax;};
static Stat measure(const vector<U>& state,int L,const array<Vel,K>& v){int V=L*L*L;vector<array<double,3>> u(V);double ek=0,et=0;int minrho=100,maxrho=0;for(int cell=0;cell<V;cell++){int rho=0,px=0,py=0,pz=0,e=0;for(int k=0;k<K;k++){int n=state[cell*K+k];rho+=n;px+=n*v[k].x;py+=n*v[k].y;pz+=n*v[k].z;e+=n*v[k].e;}minrho=std::min(minrho,rho);maxrho=std::max(maxrho,rho);et+=e;double den=std::max(rho,1);u[cell]={px/den,py/den,pz/den};ek+=(px*px+py*py+pz*pz)/(2*den);}
 auto ui=[&](int x,int y,int z,int j){x=(x+L)%L;y=(y+L)%L;z=(z+L)%L;return u[cellid(x,y,z,L)][j];};double ens=0,maxv=0;for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){double wx=(ui(x,y+1,z,2)-ui(x,y-1,z,2)-ui(x,y,z+1,1)+ui(x,y,z-1,1))/2;double wy=(ui(x,y,z+1,0)-ui(x,y,z-1,0)-ui(x+1,y,z,2)+ui(x-1,y,z,2))/2;double wz=(ui(x+1,y,z,1)-ui(x-1,y,z,1)-ui(x,y+1,z,0)+ui(x,y-1,z,0))/2;double x2=wx*wx+wy*wy+wz*wz;ens+=x2/2;maxv=std::max(maxv,std::sqrt(x2));}return {et/(2*V),ek/V,ens/V,maxv,static_cast<double>(minrho),static_cast<double>(maxrho)};}
int main(){auto v=velocities();auto normal=gen(v,0),alt=gen(v,1);bool pass=true;int differing=0;long long tests=0;for(size_t i=0;i<normal.size();i++){bool diff=false;for(const auto* g:{&normal[i],&alt[i]}){array<int,4> q{g->a,g->b,g->c,g->d};for(int c=0;c<81;c++){int d=g->lut[c];bool cond=d>=0&&d<81&&g->lut[d]==c&&inv(decode(c),q,v)==inv(decode(d),q,v);pass&=cond;tests++;}pass&=(g->lut[40]==40);}for(int c=0;c<81;c++)diff|=normal[i].lut[c]!=alt[i].lut[c];if(diff)differing++;}
 std::cout<<"GATES standard="<<normal.size()<<" alternative="<<alt.size()<<" distinct="<<differing<<" exact_invariant_checks="<<tests<<" pass="<<pass<<"\n";if(normal.size()!=288||alt.size()!=288||differing!=288||!pass)return 2;
 auto stationary=ground(16);auto next=stationary;for(int t=0;t<8;t++)tick(stationary,next,normal,v,16,t,false);std::cout<<"GROUND_STATE after_8_ticks_unchanged="<<(stationary==ground(16))<<"\n";if(stationary!=ground(16))return 3;
 auto original=tg(16,.20,v),state=original;next=state;for(int t=0;t<16;t++)tick(state,next,normal,v,16,t,false);for(int t=15;t>=0;t--)tick(state,next,normal,v,16,t,true);auto mismatch=std::mismatch(state.begin(),state.end(),original.begin());bool rev=mismatch.first==state.end();std::cout<<"REVERSIBILITY 110592-trit 16_steps restorable="<<rev<<"\n";if(!rev)return 4;
 int L=24;auto q0=ground(L), qtg=tg(L,.20,v);auto fresh=measure(qtg,L,v);std::cout<<std::setprecision(9)<<"TG_INITIAL E_total="<<fresh.energy<<" E_kin="<<fresh.kin<<" Enstrophy="<<fresh.enstrophy<<" max_vort="<<fresh.maxv<<" density="<<fresh.densityMin<<".."<<fresh.densityMax<<"\n";if(std::abs(fresh.kin-.142661)>1e-5||std::abs(fresh.enstrophy-.00133999)>1e-7)return 5;
 std::ofstream out("cpp_independent_tgv_results.csv");out<<"tick,e_total,e_kin,enstrophy,max_vorticity,density_min,density_max\n";auto s=qtg;next=s;long long conserved=checksum(s,v,4), conserved_mass=checksum(s,v,0);for(int t=0;t<=48;t++){auto m=measure(s,L,v);out<<t<<","<<std::setprecision(12)<<m.energy<<","<<m.kin<<","<<m.enstrophy<<","<<m.maxv<<","<<m.densityMin<<","<<m.densityMax<<"\n";if(checksum(s,v,0)!=conserved_mass||checksum(s,v,4)!=conserved||checksum(s,v,1)!=0||checksum(s,v,2)!=0||checksum(s,v,3)!=0)return 6;if(t==48){std::cout<<"TG_FINAL E_total="<<m.energy<<" E_kin="<<m.kin<<" Enstrophy="<<m.enstrophy<<" max_vort="<<m.maxv<<"\n";break;}tick(s,next,normal,v,L,t,false);}
 std::cout<<"PASS independent native C++ tests (not full original GitHub suite)\n";return 0;}
