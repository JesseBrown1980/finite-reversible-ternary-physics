#define main asolaria_finite_state_validation_main
#include "independent_cpp_equilibrium.cpp"
#undef main
static double hotcold(const vector<U>& s, int L,const array<Vel,K>& v){double h=0,c=0;int n=(L/2)*L*L;for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){int cell=cellid(x,y,z,L);int en=0;for(int k=0;k<K;k++)en+=s[cell*K+k]*v[k].e;if(x<L/2)h+=en;else c+=en;}return (h-c)/n;}
int main(){auto v=velocities();auto gates=gen(v,0);const int L=24,V=L*L*L;int cp=-1,cm=-1,fp=-1,fm=-1;for(int k=0;k<K;k++){auto a=v[k];if(a.x==1&&a.y==1&&a.z==1)cp=k;if(a.x==-1&&a.y==-1&&a.z==-1)cm=k;if(a.x==1&&a.y==0&&a.z==0)fp=k;if(a.x==-1&&a.y==0&&a.z==0)fm=k;}
 vector<U> s=ground(L),next=s;for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){int c=cellid(x,y,z,L)*K;int hn=(x<L/2)?2:0,cn=(x<L/2)?0:2;s[c+cp]=static_cast<U>(hn);s[c+cm]=static_cast<U>(hn);s[c+fp]=static_cast<U>(cn);s[c+fm]=static_cast<U>(cn);}
 std::ofstream heat("cpp_independent_thermal.csv");heat<<"tick,delta_T\n";for(int t=0;t<=36;t++){double d=hotcold(s,L,v);heat<<t<<","<<std::setprecision(12)<<d<<"\n";if(t==0||t==7||t==12||t==20||t==27||t==36)std::cout<<"THERMAL tick="<<t<<" delta_T="<<d<<"\n";if(t<36)tick(s,next,gates,v,L,t,false);}
 const double kv=2*std::acos(-1.0)/L;int dirs[4][3]={{1,0,0},{0,1,0},{0,0,1},{1,1,1}};const char* labels[]={"X","Y","Z","Diagonal"};std::ofstream wave("cpp_independent_isotropy_timeseries.csv");wave<<"direction,tick,signed_fourier_mass\n";
 for(int j=0;j<4;j++){int dx=dirs[j][0],dy=dirs[j][1],dz=dirs[j][2];int kp=-1,km=-1;for(int k=0;k<K;k++)if(v[k].x==dx&&v[k].y==dy&&v[k].z==dz)kp=k;else if(v[k].x==-dx&&v[k].y==-dy&&v[k].z==-dz)km=k;
 vector<double> cosfield(V);s=ground(L);next=s;for(int x=0;x<L;x++)for(int y=0;y<L;y++)for(int z=0;z<L;z++){int cell=cellid(x,y,z,L);double phi=kv*(dx*(x+.5)+dy*(y+.5)+dz*(z+.5));double cf=std::cos(phi);cosfield[cell]=cf;int base=cell*K;if(cf>0.3){s[base+kp]=2;s[base+km]=2;}else if(cf<-.3){s[base+kp]=0;s[base+km]=0;}}
 for(int t=0;t<=60;t++){double A=0;for(int c=0;c<V;c++){int rho=0;for(int k=0;k<K;k++)rho+=s[c*K+k];A+=(rho-27)*cosfield[c];}wave<<labels[j]<<","<<t<<","<<std::setprecision(14)<<A<<"\n";if(t<60)tick(s,next,gates,v,L,t,false);}std::cout<<"ISOTROPY exported="<<labels[j]<<" 61_timestamps\n";
 }
 std::cout<<"PASS native independent kinetic+isotropy probes\n";return 0;}
