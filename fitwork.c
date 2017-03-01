#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/param.h>

#define FIELDS 23


float lambda_laplacian(float std_dev){
  return sqrtf(2) / std_dev;
}

float p0_laplacian(int quantizer, float std_dev){
  return expf(-lambda_laplacian(std_dev)*quantizer);
}

float p1_laplacian(int quantizer, float std_dev, float g0){
  return expf(-lambda_laplacian(std_dev)*(g0-quantizer/2.));
}

/* corrected version of A9
float H_laplacian(int quantizer, float std_dev, float g0){
  float p = p0_laplacian(quantizer, std_dev);
  float p1 = p1_laplacian(quantizer, std_dev, g0);
  return (p1 - 1)*log2f(1 - p1)
    - p1*(log2f(p1) + log2f(1 - p) + p/(1 - p)*log2f(p) - 1);
}*/

/* derived from H12 */
float H2_laplacian(float p){
  return p*log2f(1.f/p) + (1.f - p)*log2f(1.f/(1.f-p));
}

float H_laplacian(int quantizer, float std_dev, float g0){
  float p = p0_laplacian(quantizer, std_dev);
  float p1 = p1_laplacian(quantizer, std_dev, g0);
  return H2_laplacian(p1) + p1*(1 + H2_laplacian(p)/(1-p));
}


float pdf_laplacian(float x, float std_dev){
  return expf(-sqrtf(2.f)*fabsf(x)/std_dev) / (sqrtf(2.f)*std_dev);
}

float pdf_gaussian(float x, float std_dev){
  return 1.f/(sqrt(2*M_PI)*std_dev)*expf(-x*x/(2*std_dev*std_dev));
}

float y_deadzone(int k, int quantizer, float T){
  if(k > 0)
    return T + (k-.5f)*quantizer;
  if(k < 0)
    return (k+.5f)*quantizer - T;
  return 0;
}

float l_deadzone(int k, int quantizer, float T){
  if(k)
    return y_deadzone(k, quantizer, T) - quantizer / 2.f;
  return -T;
}

float h_deadzone(int k, int quantizer, float T){
  if(k)
    return y_deadzone(k, quantizer, T) + quantizer / 2.f;
  return T;
}

float P(int k, int quantizer, float std_dev, float T,
        float(*pdf)(float, float)){
  float N = 128.f;
  float l = l_deadzone(k, quantizer, T);
  float h = h_deadzone(k, quantizer, T);
  int n = 0;
  float acc = 0;
  float i;
  float step = (h-l)/N;
  for(i=l; i<h; i+=step){
    acc += pdf(i+step/2, std_dev);
    n++;
  }
  return acc * step;
}

float H(int quantizer, float std_dev, float T,
        float(*pdf)(float, float)){
  int N = 2048;
  int i;
  float acc = 0;
  for (i= -N; i<=N; i++){
    float Pf = P(i, quantizer, std_dev, T, pdf);
    if(Pf){
      acc += Pf * log2f(Pf);
    }
  }
  return -acc;
}

float beta(int quantizer, float std_dev, float alpha, float epsilon, float T,
           float(*pdf)(float, float)){
  float scale = epsilon*std_dev/quantizer;
  return (1/(scale*scale))*expf(alpha*H(quantizer, std_dev, T, pdf));
}



int main(int argc, char **argv){
  int i;
  FILE *in;

  for(i=1; i<argc; i++){
    if(argc < 2) {
      in = stdin;
    }else{
      char *dot;
      in = fopen(argv[i],"r");
      if(in==NULL){
        fprintf(stderr,"Could not open input file %s.\n",argv[i]);
        return 1;
      }
    }

    char *bufp = NULL;
    size_t bn = 0;
    ssize_t l;
    while((l=getline(&bufp,&bn,in))>0){
      size_t ret;
      long long x[FIELDS];
      char *lptr=bufp;
      char *rptr=bufp;
      int j;
      for(j=0;j<FIELDS;j++){
        x[j]=strtoll(lptr,&rptr,10);
        if(lptr == rptr) break;
        lptr=rptr;
      }
      if(j<FIELDS){
        fprintf(stderr,"Short read in input file %s\n",argv[i]);
        continue;
      }

      /* 0: interp
         1: plane
         2: qi

         3: DC quantizer
         4: AC quantizer
         5: DC threshold
         6: AC threshold

         7: mode0
         8: mode1
         9: mode2
         10: mode3

         11: blockshape
         12: valid pixels
         13: tx type
         14: transform size
         15: coded coefficients
         16: eob fraction

         17: spatial variance sum
         18: spatial variance sum of squares
         19: distortion sum of squares
         20: SATD (minus DC component)
         21: DC component
         22: bits fraction */
      int px_n = x[12];
      float L = x[14];
      float Leob = x[15];
      float eob = x[16]<<6;
      float stddev = sqrt( (x[18] - x[17]*x[17]/px_n)/px_n);
      float satd = (float)x[20]/(L);
      int q = x[4];
      int T = x[6];
      float bits = (x[22])/(float)(1<<9);//-eob;

      if(bits > 0 && stddev>0){
        float Hval = H_laplacian(q, stddev, T+q/2.);
        float alpha = L * Hval / bits;

        printf("%f %f %f %f %f\n", alpha, satd, stddev, Leob, bits);
      }

      /************/

    }
    fclose(in);
  }
}
