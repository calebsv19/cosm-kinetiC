#include "physics/chemistry/reactor.h"
#include <math.h>
#include <string.h>
#include <limits.h>
bool chem_valid(const ChemMechanism *m) {
 if(!m || m->species<1 || m->species>CHEM_S || m->reactions<1 || m->reactions>CHEM_R || m->elements<1 || m->elements>CHEM_E) return false;
 for(int e=0;e<m->elements;e++) if(!isfinite(m->atomic_mass[e]) || m->atomic_mass[e]<=0) return false;
 for(int i=0;i<m->species;i++) {
  if(!m->id[i][0] || !memchr(m->id[i],0,32) || m->charge[i]<-100 || m->charge[i]>100) return false;
  int total=0;
  for(int e=0;e<m->elements;e++) {if(m->atoms[i][e]<0 || m->atoms[i][e]>100) return false;total+=m->atoms[i][e];}
  if(!total) return false;
  for(int j=0;j<i;j++) if(!strcmp(m->id[i],m->id[j])) return false;
 }
 for(int r=0;r<m->reactions;r++) {
  if(!isfinite(m->k[r]) || m->k[r]<0) return false;
  int order=0,products=0,charge=0,balance[CHEM_E]={0};
  for(int i=0;i<m->species;i++) {
   int a=m->reactant[r][i],b=m->product[r][i];
   if(a<0 || a>8 || b<0 || b>8) return false;
   order+=a;products+=b;charge+=(b-a)*m->charge[i];
   for(int e=0;e<m->elements;e++) balance[e]+=(b-a)*m->atoms[i][e];
  }
  if(order<1 || order>2 || products<1 || m->order[r]!=order || charge) return false;
  for(int e=0;e<m->elements;e++) if(balance[e]) return false;
 }
 return true;
}
bool chem_rates(const ChemMechanism *m,const ChemState *s,double v,double *rates) {
 if(!chem_valid(m) || !s || !rates || !isfinite(v) || v<=0) return false;
 for(int i=0;i<m->species;i++) if(!isfinite(s->amount[i]) || s->amount[i]<0) return false;
 for(int r=0;r<m->reactions;r++) {
  double q=m->k[r]*v;
  for(int i=0;i<m->species;i++) for(int j=0;j<m->reactant[r][i];j++) q*=s->amount[i]/v;
  if(!isfinite(q) || q<0) return false;
  rates[r]=q;
 }
 return true;
}
static bool derivative(const ChemMechanism *m,const ChemState *s,double v,double *d,double *rates) {
 if(!chem_rates(m,s,v,rates)) return false;
 for(int i=0;i<m->species;i++) {d[i]=0;for(int r=0;r<m->reactions;r++) d[i]+=(m->product[r][i]-m->reactant[r][i])*rates[r];if(!isfinite(d[i])) return false;}
 return true;
}
bool chem_step(const ChemMechanism *m,ChemState *s,double v,double dt) {
 if(!s || !isfinite(dt) || dt<0 || !chem_valid(m)) return false;
 ChemState c=*s,t=*s;double d[4][CHEM_S]={{0}},q[4][CHEM_R]={{0}};
 for(int r=0;r<m->reactions;r++) if(!isfinite(s->extent[r]) || s->extent[r]<0) return false;
 for(int stage=0;stage<4;stage++) {
  if(stage) for(int i=0;i<m->species;i++) t.amount[i]=s->amount[i]+dt*(stage==3?1:.5)*d[stage-1][i];
  if(!derivative(m,&t,v,d[stage],q[stage])) return false;
 }
 for(int r=0;r<m->reactions;r++) {
  double x=dt*(q[0][r]+2*q[1][r]+2*q[2][r]+q[3][r])/6;
  c.extent[r]+=x;if(!isfinite(c.extent[r])) return false;
  for(int i=0;i<m->species;i++) c.amount[i]+=(m->product[r][i]-m->reactant[r][i])*x;
 }
 double check[CHEM_R];if(!chem_rates(m,&c,v,check)) return false;
 /* Independent element/charge totals: reject, never repair, a broken budget. */
 for(int e=0;e<=m->elements;e++) {
  double a=0,b=0;
  for(int i=0;i<m->species;i++) {int w=e==m->elements?m->charge[i]:m->atoms[i][e];a+=w*s->amount[i];b+=w*c.amount[i];}
  if(!isfinite(a) || !isfinite(b) || fabs(a-b)>1e-12*fmax(1,fabs(a))) return false;
 }
 *s=c;return true;
}
