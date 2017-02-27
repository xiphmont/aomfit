#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <sys/param.h>

#define FIELDS 18

/* dimensions: inter_p, qindex, plane, mode[4], bsize, txsize */

typedef struct {
  char *name;
  int ext0;
  int ext1;
  int pos;
  int posval;
} argument;

static argument arguments[] = {
  { "inter", 0, 0,
    0, 1},
  { "intra", 1, 1,
    0, 0},
  { "frametype", 0, 1,
    0, -1},
  { "Y", 3, 3,
    1, 0},
  { "U", 4, 4,
    1, 1},
  { "V", 5, 5,
    1, 2},
  { "plane", 3, 5,
    1, -1},
  { "qi", 7, 7,
    2, -1},
  { "mode", 8, 8,
    4, -1},
  { "modeB", 9, 9,
    5, -1},
  { "modeC", 10, 10,
    6, -1},
  { "modeD", 11, 11,
    7, -1},
  { "blockshape", 12, 12,
    8, -1},
  { "pixels", 13, 13,
    9, -1},
  { "N", 13, 13,
    9, -1},
  { "txtype", 15, 15,
    10, -1},
  { "txsize", 16, 16,
    11, -1},
  { "L", 16, 16,
    11, -1},
  { "eobcount", 18, 18,
    12, -1},
  { NULL }
};

static argument *a = arguments;
static char *base = NULL;

FILE *sfopen(char *mode, int posval){
  char buf[MAXPATHLEN];
  int range = a->ext1 - a->ext0 + 1;
  if (range > 1) {
    if(posval >= range){
      fprintf(stderr,"Position value out of expected range in data:\n");
      fprintf(stderr," argument name: %s\n  possible range: 0-%d\n  "
              "  value: %d\n",a->name, a->ext1-a->ext0, posval);
      return NULL;
    }
    if(a->posval >= 0){
      fprintf(stderr,"Invalid argument entry in list:\n");
      fprintf(stderr," argument name: %s\n  possible range: 0-%d\n  "
              "  value: %d\n",a->name, a->ext1-a->ext0, a->posval);
      return NULL;
    }
    snprintf(buf,MAXPATHLEN,"%s%s%s.m",
            base,
            *base?"-":"",
            arguments[a->ext0+posval].name);
  }else{
    if(a->posval < 0){
      snprintf(buf,MAXPATHLEN,"%s%s%s%d.m",
              base,
              *base?"-":"",
              arguments[a->ext0].name,posval);
    }else{
      if(a->posval == posval){
        snprintf(buf,MAXPATHLEN,"%s%s%s.m",
                base,
                *base?"-":"",
                arguments[a->ext0].name);
      }
    }
  }
  FILE *f = fopen(buf,mode);
  if(f==NULL){
    fprintf(stderr,"Could not open output file %s.\n", buf);
    exit(1);;
  }
  return f;
}

FILE **out=NULL;
int out_active = 0;
int out_min = 0;
int out_max = 0;
int out_size = 0;

int sfwrite(char *buf, int n, int stream){
  if(out == NULL){
    /* No output files actually opened, structures not allocated */
    out_max = stream + 128;
    out = calloc(out_max, sizeof(*out));
  }
  if(out_size < stream+1){
    /* reallocate to larger output structure */
    int new = stream + 128;
    out = realloc(out, new*sizeof(*out));
    memset(out+out_size, 0, (new-out_size)*sizeof(*out));
    out_size = new;
  }

  if(!out[stream]){
    /* Do not currently have an output stream for this position index */
    while(out_active >= 128){
      /* Have too many output streams open... cull one*/
      if(stream < out_min){
        /* Cull one off the top */
        out_max--;
        if(out[out_max]){
          fclose(out[out_max]);
          out[out_max] = 0;
          out_active--;
        }
        continue;
      }
      if(out[out_min]){
        /* cull one off the bottom */
        fclose(out[out_min]);
        out[out_min] = 0;
        out_active--;
      }
      out_min++;
    }
    out[stream] = sfopen("a",stream);
    if(!out[stream]) return -1;
    if(stream < out_min) out_min = stream;
    if(stream >= out_max) out_max = stream+1;
    out_active++;
  }

  return fwrite(buf, 1, n, out[stream]);
}

void sfclose(void){
  int i;
  if(out){
    for(i=out_min; i<out_max; i++){
      if(out[i])fclose(out[i]);
    }
    free(out);
    out=NULL;
    out_min=0;
    out_max=0;
    out_size=0;
  }
}

int main(int argc, char **argv){
  int i;
  FILE *in;
  if(argc < 2) {
    fprintf(stderr,"Partition type is a required argument.\n");
    return 1;
  }
  int pos = -1;
  int posval = -1;
  while(a->name){
    if(!strcmp(a->name,argv[1]))break;
    a++;
  }
  if(!a->name){
    fprintf(stderr,"Unknown partition request: %s\n",argv[1]);
    return 1;
  }
  pos = a->pos;
  posval = a->posval;

  for(i=2; i==2 || i<argc; i++){
    if(argc < 3) {
      in = stdin;
      base = strdup("");
    }else{
      char *dot;
      in = fopen(argv[i],"r");
      if(in==NULL){
        fprintf(stderr,"Could not open input file %s.\n",argv[i]);
        return 1;
      }
      if(argc == 3){
        base = strdup(basename(argv[i]));
        dot = strrchr(base, '.');
        if (dot) *dot=0;
      }else{
        base = strdup("");
      }
    }

    char *bufp = NULL;
    char *tokp = NULL;
    size_t bn = 0;
    size_t tn = 0;
    ssize_t l;
    int toklength=0;
    while((l=getline(&bufp,&bn,in))>0){
      size_t ret;
      if(bufp[0]=='#'){
        char *tmpbuf = bufp;
        ssize_t tempn = bn;
        bufp = tokp;
        tokp = tmpbuf;
        bn = tn;
        tn = tempn;
        toklength = l;
      } else {
        unsigned long long x;
        char *lptr=bufp;
        char *rptr=bufp;
        int j;
        for(j=0;j<a->pos;j++){
          strtoull(lptr,&rptr,10);
          if(lptr == rptr) break;
          lptr=rptr;
        }
        x=strtoull(lptr,&rptr,10);
        if(lptr == rptr) continue;
        if(posval == -1 || (int)x == posval){
          if(toklength) ret=sfwrite(tokp,toklength,x);
          ret=sfwrite(bufp,l,x);
          if(ret!=l){
            fprintf(stderr,"Write failed (%d != %d)\n",ret,l);
            return 1;
          }
        }
        toklength = 0;
      }
    }
    fclose(in);
  }
  sfclose();
}
