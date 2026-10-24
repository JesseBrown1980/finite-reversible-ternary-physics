#define main previous_audit_main
#include "independent_multishell_base.cpp"
#undef main
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
int main(){
 const auto face=facelut(); const auto edge=edgelut(); const auto d=build_cross(); const auto oh=makeoh();
 cout<<"INDEPENDENT INTER-SHELL ENGINE\ncorner_quadruplets="<<d.corner.size()<<" rest_edge_pairs="<<d.ef.size()<<"\n";
 mt19937_64 rng(88937ULL);uniform_int_distribution<int> tri(0,2);
 int active=0,revFail=0,invFail=0,eqFail=0;S example{};bool saved=false;
 const int trials=5000;
 for(int t=0;t<trials;t++){
  S s{};for(uint8_t&x:s)x=uint8_t(tri(rng));
  S z=full(s,face,edge,d);active+=(s!=z);invFail+=vals(s)!=vals(z);revFail+=backward(z,face,edge,d)!=s;
  if(!saved&&backward(z,face,edge,d)!=s){example=s;saved=true;}
  for(const M&m:oh)eqFail+=full(rotcell(s,m),face,edge,d)!=rotcell(z,m);
 }
 cout<<"local_states="<<trials<<" active="<<active<<" conservation_failures="<<invFail<<" inverse_failures="<<revFail<<" group_equivariance_failures="<<eqFail<<"/"<<trials*oh.size()<<"\n";
 if(saved){cout<<"first inverse counterexample:";for(uint8_t x:example)cout<<int(x);cout<<"\n";}
 // Uniform random sample over 24^3 lattice evolved 48 steps
 int L=24;State st24(L*L*L);for(S&c:st24)for(uint8_t&x:c)x=uint8_t(tri(rng));auto totals=[](const State &state){array<long long,4>a{};for(const S&s:state){auto sh=shells(s);for(int k=0;k<4;k++)a[k]+=sh[k];}return a;};
 auto ini=totals(st24);for(int t=0;t<48;t++)st24=fullstep(st24,L,face,edge,d);auto fin=totals(st24);cout<<"shell totals at tick0:";for(auto x:ini)cout<<" "<<x;cout<<"\nshell totals at tick48:";for(auto x:fin)cout<<" "<<x;cout<<"\n";
 long long lm=0;
 {
 int L=8;State st(L*L*L);for(S&c:st)for(uint8_t&x:c)x=uint8_t(tri(rng));State z=fullstep(fullstep(st,L,face,edge,d),L,face,edge,d);for(const M&m:oh){State lhs=fullstep(fullstep(rotgrid(st,L,m),L,face,edge,d),L,face,edge,d),rhs=rotgrid(z,L,m);for(size_t i=0;i<z.size();i++)for(int k=0;k<27;k++)lm+=lhs[i][k]!=rhs[i][k];}
 cout<<"whole_lattice_L="<<L<<" rotations="<<oh.size()<<" trit_checks="<<int(oh.size())*L*L*L*27<<" mismatches="<<lm<<"\n";
 }
 long long mass=0,energy=0;for(int k=0;k<4;k++){mass+=fin[k]-ini[k];energy+=k*(fin[k]-ini[k]);}cout<<"mass_delta="<<mass<<" energy_delta="<<energy<<"\n";

 // Independently replay the published "constant-density thermal slab" initialization.
 L=24; State thermal(L*L*L);
 for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){
  S cell{};cell.fill(1);
  if(x<L/2){cell[22]=cell[4]=2;cell[16]=cell[10]=cell[14]=cell[12]=0;}
  else{cell[22]=cell[4]=0;cell[16]=cell[10]=cell[14]=cell[12]=1;}
  cell[13]=2;cell[1]=2;
  thermal[idx(x,y,z,L)]=cell;
 }
 auto thobs=[&](const State& ss, int t){
  long long nmin=100000,nmax=-1,emin=10000,emax=-1,ehot=0,ecold=0;array<long long,3> p={};
  for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){
   auto a=vals(ss[idx(x,y,z,L)]);nmin=min(nmin,(long long)a[0]);nmax=max(nmax,(long long)a[0]);emin=min(emin,(long long)a[4]);emax=max(emax,(long long)a[4]);
   if(x<L/2)ehot+=a[4];else ecold+=a[4];for(int j=0;j<3;j++)p[j]+=a[j+1];
  }
  cout<<"thermal_tick="<<t<<" density_range="<<nmin<<".."<<nmax<<" energy_range="<<emin<<".."<<emax<<" mean_half_E_hot="<<double(ehot)/(L*L*L/2)<<" mean_half_E_cold="<<double(ecold)/(L*L*L/2)<<" momentum_total="<<p[0]<<","<<p[1]<<","<<p[2]<<"\n";
 };
 thobs(thermal,0);
 int thermActive=0;for(const S &cell:thermal)thermActive+=full(cell,face,edge,d)!=cell;
 cout<<"thermal_initial_collision_active="<<thermActive<<"/"<<L*L*L<<"\n";
 for(int t=1;t<=48;t++){thermal=fullstep(thermal,L,face,edge,d);if(t==1||t==6||t==12||t==48)thobs(thermal,t);}

 // New falsifiable control: equal density, equal zero momentum, but deliberately unequal initial energies.
 // Hot face (+x,-x) 0, edges (+x,+y),(-x,-y) 2 -> delta E +2 relative to all ones.
 // Cold face (+x,-x) 2, edges (+x,+y),(-x,-y) 0 -> delta E -2.
 State realheat(L*L*L);
 for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){
  S q{};q.fill(1);
  if(x<L/2){q[22]=q[4]=0; q[1]=q[25]=2;}
  else {q[22]=q[4]=2; q[1]=q[25]=0;}
  realheat[idx(x,y,z,L)]=q;
 }
 ofstream cfile("corrected_thermal_timeseries.csv");
 cfile<<"tick,mean_energy_hot,mean_energy_cold,delta_energy,mean_density_hot,mean_density_cold,net_px,net_py,net_pz\n";
 for(int t=0;t<=48;t++){
  long long eh=0,ec=0,nh=0,nc=0;array<long long,3> pp={};
  for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){
   auto a=vals(realheat[idx(x,y,z,L)]);if(x<L/2){eh+=a[4];nh+=a[0];}else{ec+=a[4];nc+=a[0];}
   for(int j=0;j<3;j++)pp[j]+=a[j+1];
  }
  double vol=double(L*L*L/2);
  cfile<<t<<","<<double(eh)/vol<<","<<double(ec)/vol<<","<<double(eh-ec)/vol<<","<<double(nh)/vol<<","<<double(nc)/vol<<","<<pp[0]<<","<<pp[1]<<","<<pp[2]<<"\n";
  if(t==0||t==1||t==6||t==12||t==24||t==48)cout<<"CORRECTED_HEAT t="<<t<<" mean_E_hot="<<double(eh)/vol<<" mean_E_cold="<<double(ec)/vol<<" delta="<<double(eh-ec)/vol<<" mean_rho="<<double(nh)/vol<<","<<double(nc)/vol<<" netP="<<pp[0]<<","<<pp[1]<<","<<pp[2]<<"\n";
  if(t<48)realheat=fullstep(realheat,L,face,edge,d);
 }
 if(invFail||revFail||eqFail||lm||mass||energy) return 1;
 return 0;
}
