/* Diagnostic only: actual backward-Euler x-face flux before the z solve.
 * Cumulative joules cover source-height z band and full physical y span.
 * Snapshots use accepted state after complete exchange windows. */
#ifndef PHYSICS_SIM_LOCAL_INTERFACE_TRACE_H
#define PHYSICS_SIM_LOCAL_INTERFACE_TRACE_H
static const int interface_faces[]={46,47,48,49,50,63,64,65};
static double interface_adv[8],interface_diff[8];
static int interface_tick;
static void physics_sim_interface_flux(int k,double dt){
 double overlap=fmax(0,fmin(1./32,(k+1)*dz)-fmax(1./64,k*dz));
 if(!overlap)return;
 const double *v=candidate+k*nf;double scale=dt*1206*.25*overlap,u=.05*(k+.5)/nz;
 for(int p=0;p<8;p++){
  int j=offset[interface_faces[p]],left=j-1;
  interface_adv[p]+=scale*u*v[left];
  interface_diff[p]-=scale*alpha*(v[j]-v[left])/(.5*(width[j]+width[left]));
 }
}
static void physics_sim_interface_profile(void){
 if(interface_tick%40&&interface_tick!=780&&interface_tick!=785)return;
 fprintf(stderr,"{\"tick\":%d,\"fine_flux_end_s\":%.17g,\"faces\":[",interface_tick,fmin(interface_tick,782)/1600.);
 for(int p=0;p<8;p++){
  int x=interface_faces[p],j=offset[x];
  fprintf(stderr,"%s{\"parent\":%d,\"x_m\":%.17g,\"left_width_m\":%.17g,\"right_width_m\":%.17g,\"advected_j\":%.17g,\"diffused_j\":%.17g}",p?",":"",x,x*dx,width[j-1],width[j],interface_adv[p],interface_diff[p]);
 }
 fprintf(stderr,"],\"profiles\":[");
 for(int x=44;x<=68;x++){
  double band=0,near=0;
  for(int k=0;k<nz;k++){
   double value=parent_mean(accepted,k,x,accepted_n);
   band+=value*fmax(0,fmin(1./32,(k+1)*dz)-fmax(1./64,k*dz))/(1./64);
   near+=value*fmax(0,fmin(1./128,(k+1)*dz)-k*dz)/(1./128);
  }
  fprintf(stderr,"%s{\"parent\":%d,\"band_k\":%.17g,\"near_k\":%.17g}",x==44?"":",",x,band,near);
 }
 fprintf(stderr,"]}\n");
}
#endif
