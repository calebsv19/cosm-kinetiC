/* Cached SPD preconditioner for the unchanged masked pressure equation.
 * CG acceptance still uses the independent matrix-free true residual. */
#include "app/cfd_open_atmosphere3d.h"
CfdSparseMg *cfd_open_atmosphere3d_box_preconditioner(CfdOpenAtmosphere3d *s) {
    int n=s->grid.count,used=0;
    int *row=cfd_memory_calloc((size_t)n+1,sizeof(int)),*col=cfd_memory_malloc((size_t)7*n*sizeof(int));
    double *value=cfd_memory_malloc((size_t)7*n*sizeof(double)),*xyz=cfd_memory_malloc((size_t)3*n*sizeof(double));
    CfdSparseMg *mg=NULL;
    if(!row || !col || !value || !xyz)goto done;
    for(int q=0;q<n;q++) {
        double p[3];cfd_cartesian3d_position(&s->grid,q,-1,p);for(int a=0;a<3;a++)xyz[a*n+q]=p[a];
        int columns[7]={q},count=1;double values[7]={0};
        if(cfd_open_atmosphere3d_solid(s,q))values[0]=1;
        else for(int a=0;a<3;a++)for(int side=-1;side<=1;side+=2) {
            int stride=(int[]){1,s->grid.n[0],s->plane}[a],coord=(q/stride)%s->grid.n[a];
            double w=1/(s->grid.h[a]*s->grid.h[a]);
            bool edge=(side<0 && coord==0) || (side>0 && coord==s->grid.n[a]-1);
            if(edge && (a==2 || s->lateral_open)){
                if(a!=2 || side>0 || !s->ground)values[0]+=2*w;
                continue;
            }
            int other=a==2?q+side*s->plane:cfd_cartesian3d_neighbor(&s->grid,q,a,side);
            if(!cfd_open_atmosphere3d_solid(s,other)){values[0]+=w;columns[count]=other;values[count++]=-w;}
        }
        for(int i=1;i<count;i++)for(int j=i;j>0 && columns[j]<columns[j-1];j--){int c=columns[j];columns[j]=columns[j-1];columns[j-1]=c;double v=values[j];values[j]=values[j-1];values[j-1]=v;}
        row[q]=used;for(int i=0;i<count;i++){col[used]=columns[i];value[used++]=values[i];}
    }
    row[n]=used;mg=cfd_sparse_mg_create3d(n,row,col,value,xyz,xyz+n,xyz+2*n,2*s->grid.h[0],2*s->grid.h[1],2*s->grid.h[2]);
    if(mg){s->solid_row=row;s->solid_col=col;s->solid_values=value;row=NULL;col=NULL;value=NULL;}
 done:
    cfd_memory_free(row);cfd_memory_free(col);cfd_memory_free(value);cfd_memory_free(xyz);return mg;
}
