import os
import pandas as pd,numpy as np
from scipy.optimize import minimize
p = './'
if not os.path.exists(p + 'cpp_independent_isotropy_timeseries.csv') and os.path.exists('/mnt/data/asolaria_local_validation/cpp_independent_isotropy_timeseries.csv'):
    p = '/mnt/data/asolaria_local_validation/'
df=pd.read_csv(p+'cpp_independent_isotropy_timeseries.csv')
k0=2*np.pi/24
results=[]
for direction,part in df.groupby('direction',sort=False):
 t=part['tick'].to_numpy(dtype=float); y=part['signed_fourier_mass'].to_numpy(); ym=y.mean(); sst=((y-ym)**2).sum()
 k=k0*(np.sqrt(3) if direction=='Diagonal' else 1)
 def fit(w,alpha):
  E=np.exp(-alpha*t)
  mat=np.column_stack((np.ones(len(t)), E*np.cos(w*t),E*np.sin(w*t)))
  coef=np.linalg.lstsq(mat,y,rcond=None)[0]
  rss=np.sum((y-mat@coef)**2)
  return rss,coef
 def objective(z):return fit(z[0],z[1])[0]
 best=None
 for w0 in np.linspace(k*0.65,k*1.25,9):
  for a0 in [.001,.01,.03,.06]:
   m=minimize(objective,[w0,a0],method='L-BFGS-B',bounds=[(k*.5,k*1.5),(0,.15)],options={'ftol':1e-12,'maxiter':160})
   if best is None or m.fun<best.fun:best=m
 omega,alpha=best.x;rss,coef=fit(omega,alpha)
 results.append(dict(direction=direction,k=k,omega=omega,cs=omega/k,alpha=alpha,r2=1-rss/sst,baseline=coef[0],A=coef[1],B=coef[2],start_amp=y[0]))
 res=results[-1]
 print(direction,'k=',round(k,6),'freq=',round(omega,6),'cs=',round(omega/k,6),'alpha=',round(alpha,6),'R2=',round(res['r2'],6))
out=pd.DataFrame(results)
print('anisotropy', (out.cs.max()-out.cs.min())/out.cs.mean()*100)
print('mean_speed',out.cs.mean())
out.to_csv(p+'cpp_independent_isotropy_refit.csv',index=False)
