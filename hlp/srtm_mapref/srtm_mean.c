#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>

#define SDIM	1201
#define DDIM	12
#define MDIM	((SDIM-1)/DDIM)

struct arg {
	short *map;
	char fn[64];
	int gx,gy;
	char run;
	pthread_t pt;
};

void *loadfile(void *arg){
	struct arg *a=(struct arg*)arg;
	short *buf=malloc(SDIM*SDIM*sizeof(short));
	int i,x,y;
	FILE *fd=popen(a->fn,"r");
	unsigned char *b;
	size_t r;
	if(SDIM*SDIM!=(r=fread(buf,sizeof(short),SDIM*SDIM,fd))){
		printf("ERROR: read only %lu of %i elements\n",r,SDIM*SDIM);
		exit(1);
	}
	pclose(fd);
	for(b=(unsigned char*)buf,i=0;i<SDIM*SDIM;i++,b+=2){
		unsigned char t=b[1]; b[1]=b[0]; b[0]=t;
	}
	for(y=0;y<DDIM;y++) for(x=0;x<DDIM;x++){
		double s=0.; int n=0;
		int xi,yi;
		short v;
		for(yi=0;yi<MDIM;yi++) for(xi=0;xi<MDIM;xi++)
			if((v=buf[(y*MDIM+yi)*SDIM+(x*MDIM+xi)])!=-32768){
				s+=(double)v;
				n++;
			}
		a->map[((89-a->gy)*DDIM+y)*360*DDIM+(a->gx+180)*DDIM+x]=(short)round(s/(double)n);
/*		printf("%ix%i + %ix%i => %ix%i => %i => %.1f/%i => %i\n",
			a->gx,a->gy,
			x,y,
			(a->gy+90)*DDIM+y,(a->gx+180)*DDIM+x,
			((a->gy+90)*DDIM+y)*360*DDIM+(a->gx+180)*DDIM+x,
			s,n,
			(short)round(s/(double)n));*/
	}
	free(buf);
	return NULL;
}

void savemap(short *map){
	FILE *fd;
	char cmd[128];
	fd=fopen("map","wb");
	fwrite(map,sizeof(short),360*DDIM*180*DDIM,fd);
	fclose(fd);
	snprintf(cmd,128,"rawtopgm -bpp 2 %i %i <map >map.pgm",360*DDIM,180*DDIM);
	printf("##%s\n",cmd);
	system(cmd);
}

#define NT	10

void loaddir(short *map,const char *dn){
	DIR *dd=opendir(dn);
	struct dirent *d;
	int n=0,t;
	struct arg pt[NT];
	for(t=0;t<NT;t++){ pt[t].run=0; pt[t].map=map; }
	if(!dd) exit(1);
	t=0;
	while((d=readdir(dd))) if(!(d->d_type&DT_DIR)){
		if(pt[t].run) pthread_join(pt[t].pt,NULL);
		pt[t].gx=atoi(d->d_name+4); if(d->d_name[3]=='W') pt[t].gx*=-1;
		pt[t].gy=atoi(d->d_name+1); if(d->d_name[0]=='S') pt[t].gy*=-1;
		printf("##file %5i: %s (%4ix%4i) \n",n++,d->d_name,pt[t].gx,pt[t].gy);
		snprintf(pt[t].fn,64,"xz -cd %s/%s",dn,d->d_name);
		pthread_create(&pt[t].pt,NULL,loadfile,pt+t);
		pt[t].run=1;
		if(!(n%500)) savemap(map);
		t=(t+1)%NT;
	}
	closedir(dd);
	for(t=0;t<NT;t++) if(pt[t].run) pthread_join(pt[t].pt,NULL);
}


int main(){
	short *map=calloc(360*DDIM*180*DDIM,sizeof(short));
	loaddir(map,"dat");
	savemap(map);
	return 0;
}
