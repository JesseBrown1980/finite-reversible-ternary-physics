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
int main(){auto oh=makeoh();auto face=facelut();auto edge=edgelut();int active=0,invfail=0,eqfail=0;for(int k=0;k<729;k++){if(face[k]!=k)active++;if(face[face[k]]!=k)invfail++;for(const auto&m:oh)if(face[code6(rotate6(dec6(k),m))]!=code6(rotate6(dec6(face[k]),m)))eqfail++;}cout<<"Face states 729, active "<<active<<", involution failures "<<invfail<<", Oh failures "<<eqfail<<"\n";
array<int,3> edge_active{};for(int g=0;g<3;g++)for(int k=0;k<81;k++)edge_active[g]+=edge[g][k]!=k;
double prob_quiet=double(729-active)/729.0;for(int g=0;g<3;g++)prob_quiet*=double(81-edge_active[g])/81.0;
cout<<"Edge gate active patterns "<<edge_active[0]<<","<<edge_active[1]<<","<<edge_active[2]<<" of 81; analytically predicted random full-cell collision activation "<<100*(1-prob_quiet)<<"%\n";if(active!=180||invfail||eqfail)return 1;
mt19937_64 rng(88937);int fails=0,live=0;for(int i=0;i<5000;i++){S s{};for(uint8_t&x:s)x=rng()%3;S t=update(s,face,edge);live+=s!=t;fails+=vals(s)!=vals(t);fails+=shells(s)!=shells(t);fails+=update(t,face,edge)!=s;for(const M&m:oh)fails+=update(rotcell(s,m),face,edge)!=rotcell(t,m);}cout<<"Random full-cell tests 5000x48; active "<<live<<", failures "<<fails<<"\n";if(fails)return 2;
int L=8;State s(L*L*L);for(S&a:s)for(uint8_t&x:a)x=rng()%3;State twice=step(step(s,L,face,edge),L,face,edge);long long diffs=0;for(const M&m:oh){State lhs=step(step(rotgrid(s,L,m),L,face,edge),L,face,edge);State rhs=rotgrid(twice,L,m);for(size_t j=0;j<lhs.size();j++)for(int k=0;k<27;k++)diffs+=lhs[j][k]!=rhs[j][k];}cout<<"Whole-lattice two-step Oh equivariance comparisons "<<48*L*L*L*27<<", failures "<<diffs<<"\n";if(diffs)return 3;
S a{};a.fill(1);a[22]=2;a[4]=2;S b=update(a,face,edge);cout<<"Face sound probe changes trits: "<<int(a!=b)<<"\n";
// Reconstruct exactly the test_multishell_equivariance.cpp slab initial conditions.
int n=24;State hotcold(n*n*n);long long localMassMin=9999,localMassMax=0;array<long long,3> netP{};
for(int x=0;x<n;x++)for(int y=0;y<n;y++)for(int z=0;z<n;z++){
 S a{};a.fill(1);bool hot=x<n/2;
 a[22]=a[4]=a[1]=a[7]=uint8_t(hot?2:0);hotcold[idx(x,y,z,n)]=a;
 auto obs=vals(a);localMassMin=min(localMassMin,(long long)obs[0]);localMassMax=max(localMassMax,(long long)obs[0]);
 for(int j=0;j<3;j++)netP[j]+=obs[j+1];
}
cout<<"Thermal-test initial density range "<<localMassMin<<".."<<localMassMax<<", global momentum ("<<netP[0]<<","<<netP[1]<<","<<netP[2]<<")\n";
int slabActive=0,faceActive=0;
for(int x=0;x<n;x++)for(int y=0;y<n;y++)for(int z=0;z<n;z++){
 S slab=hotcold[idx(x,y,z,n)];slabActive+=update(slab,face,edge)!=slab;
 S facestate{};facestate.fill(1);if(x%4==0){facestate[22]=2;facestate[4]=2;}faceActive+=update(facestate,face,edge)!=facestate;
}
cout<<"Initial active collision cells: face pulse "<<faceActive<<"/"<<n*n*n<<", hot-cold slab "<<slabActive<<"/"<<n*n*n<<"\n";

auto glob=[&](const State &st){array<long long,4> sums{};for(const auto &cell:st){auto q=shells(cell);for(int k=0;k<4;k++)sums[k]+=q[k];}return sums;};
auto shel0=glob(hotcold);auto sh=hotcold;
for(int t=0;t<48;t++)sh=step(sh,n,face,edge);
auto shel48=glob(sh);cout<<"Four velocity-shell total occupancy before ("<<shel0[0]<<","<<shel0[1]<<","<<shel0[2]<<","<<shel0[3]<<") after ("<<shel48[0]<<","<<shel48[1]<<","<<shel48[2]<<","<<shel48[3]<<")\n";
if(shel0!=shel48)return 4;
cout<<"SUCCESS\n";return 0;}
