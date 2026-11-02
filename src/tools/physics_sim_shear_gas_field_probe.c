/* Read-only frozen normal flux of a completed gas-only field. No advancement. */
#include "physics_sim_tail_diffusion.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static double field[2112*256],width[2112],normal[256],power[256],row[256],mean[256];
static PhysicsSimTailDiffusion z;
int main(int argc,char **argv){
 if(argc!=2)return 2;FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(field,sizeof(double),2112*256,f)==2112*256);assert(fgetc(f)==EOF);fclose(f);
 int n=0;for(int x=0;x<128;x++){int count=x<48||x>=112?32:1;for(int j=0;j<count;j++)width[n++]=.25/128/count;}assert(n==2112);
 for(int k=0;k<256;k++){normal[k]=.0625/256;power[k]=k>=64&&k<128;}
 assert(physics_sim_tail_diffusion_init(&z,256,normal,false));assert(physics_sim_tail_heater_source(&z,power,.026/1206));
 double integrated[2]={0},central[2]={0};int faces[2]={1,32};
 for(int j=0;j<2112;j++){
  double center=0;for(int q=0;q<j;q++)center+=width[q];center+=width[j]/2;
  z.heater_power=center>=1./32&&center<1./16?power:NULL;
  for(int k=0;k<256;k++){row[k]=field[k*2112+j];mean[k]+=row[k]*width[j]/.25;}
  for(int i=0;i<2;i++){integrated[i]+=physics_sim_tail_gradient(&z,row,faces[i],2)*width[j]/.25;central[i]+=physics_sim_tail_gradient(&z,row,faces[i],0)*width[j]/.25;}
 }
 for(int k=0;k<256;k++)power[k]/=8;z.heater_power=power;
 printf("{\"first_face_gradient\":%.17g,\"band_face_gradient\":%.17g,\"first_face_central\":%.17g,\"band_face_central\":%.17g,\"mean_first_gradient\":%.17g,\"mean_band_gradient\":%.17g}\n",integrated[0],integrated[1],central[0],central[1],physics_sim_tail_gradient(&z,mean,1,2),physics_sim_tail_gradient(&z,mean,32,2));
 for(int k=0;k<256;k++)printf("{\"normal_cell\":%d,\"mean_k\":%.17g}\n",k,mean[k]);
}
