#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

/****************************************************/

#define NMAXALL
/*#define NMAX32K*/

/*#define DIV2*/
#define DIV8

#define NZERO

#define DDIM	12

/****************************************************/

#ifdef DIV2
	#define DIV 2
#elif defined DIV8
	#define DIV 8
#endif
#define ADD		512
#define ZERO	(ADD/DIV)

#define XD		(360*DDIM)
#define YD		(180*DDIM)

short map[XD*YD];

void err(int p,int m,int v){
	if(m<0) printf("ERROR length: %i->%i\n",XD*YD,p);
	else printf("ERROR at %i: %i->%i\n",p,m,v);
	exit(1);
}

void test(char *fn){
	unsigned int v=0;
	int t=0,i=0;
	short *m=map;
	FILE *fd=fopen(fn,"r");
	#ifdef NZERO
	while(m-map<XD*YD){
		int n;
		v=0;
		if(3!=fread(&v,1,3,fd)) err(m-map,-1,-1);
		n=v+1;
		for(i=0;i<n;i++) if(*(m++)!=ZERO) err(m-map-1,*(m-1),ZERO);
		if(m-map==XD*YD) break;
		n=3;
		while(n==3){
			int v0,v1,v2;
			v=0;
			if(4!=fread(&v,1,4,fd)) err(m-map,-1,-1);
			v0=v&((1<<10)-1);
			v1=(v>>10)&((1<<10)-1);
			v2=(v>>20)&((1<<10)-1);
			n=(v>>30);
			if(*(m++)!=v0) err(m-map-1,*(m-1),v0);
			if(n>=1 && *(m++)!=v1) err(m-map-1,*(m-1),v1);
			if(n>=2 && *(m++)!=v2) err(m-map-1,*(m-1),v2);
		}
	}
	#else
	#ifdef NMAXALL
	while(3==fread(&v,1,3,fd)){
		int n=v+1;
	#elif defined NMAX32K
	while(2==fread(&v,1,2,fd)){
		int n=(v&32767)+1;
		t=(v&32768);
	#endif
		#ifdef DIV2
		for(i=0;i<n;i+=2){
			int v0,v1;
			if(t){
				v=0;
				fread(&v,1,3,fd);
				v0=v&((1<<12)-1);
				v1=v>>12;
			}else v0=v1=ZERO;
			if(*(m++)!=v0) err(m-map-1,*(m-1),v0);
			if(i+1<n && *(m++)!=v1) err(m-map-1,*(m-1),v1);
		}
		#elif defined DIV8
		for(i=0;i<n;i+=3){
			int v0,v1,v2;
			if(t){
				fread(&v,1,4,fd);
				v0=v&((1<<10)-1);
				v1=(v>>10)&((1<<10)-1);
				v2=(v>>20)&((1<<10)-1);
			}else v0=v1=v2=ZERO;
			if(*(m++)!=v0) err(m-map-1,*(m-1),v0);
			if(i+1<n && *(m++)!=v1) err(m-map-1,*(m-1),v1);
			if(i+2<n && *(m++)!=v2) err(m-map-1,*(m-1),v2);
		}
		#endif
		t=!t;
		v=0;
	}
	#endif
	fclose(fd);
	if(m-map!=XD*YD) err(m-map,-1,-1);
	printf("OK (%li)\n",m-map);
}

int vmin;
int vmax;
int nmax=0;
int nnum=0;
int vnum=0;
FILE *fd;
short *m;

void write(int n,int t){
	unsigned int v=n-1;
	nnum++; if(n>nmax) nmax=n; if(t) vnum+=n;
	/*printf("%7i: %i\n",n,t);*/
	#ifdef NMAXALL
		#ifdef NZERO
		if(!t)
		#endif
	fwrite(&v,1,3,fd);
	#elif defined NMAX32K
		#ifdef NZERO
		if(*m==ZERO) v+=32768;
		#else
		if(t) v+=32768;
		#endif
	fwrite(&v,1,2,fd);
	#endif
	if(t){
		int i;
		short *mi=m-n;
		#ifdef DIV2
		for(i=0;i<n;i+=2,mi+=2){
			v=mi[0];
			if(i+1<n) v+=mi[1]<<12;
			else v+=mi[0]<<12;
			fwrite(&v,1,3,fd);
		}
		#elif defined DIV8
		for(i=0;i<n;i+=3,mi+=3){
			v=mi[0];
			if(i+1<n) v+=mi[1]<<10;
			if(i+2<n) v+=mi[2]<<20;
			#ifdef NZERO
			if(n-i<4) v+=(n-i-1)<<30;
			else v+=3<<30;
			#endif
			fwrite(&v,1,4,fd);
		}
		#endif
	}
}

int main(){
	int i;
	int t,n;
	char fn[16],fnc[16];;
	fd=fopen("mapf","r");
	fread(map,sizeof(short),XD*YD,fd);
	fclose(fd);
	for(i=0,m=map;i<XD*YD;i++,m++) *m=(*m+ADD+(DIV/2))/DIV;
	t=map[0]!=ZERO; n=1; m=map+1; vmin=vmax=map[0];
	snprintf(fnc,16,"mapc%c%i%s",
		#ifdef NMAXALL
		'a'
		#elif defined NMAX32K
		'3'
		#endif
		,DIV,
		#ifdef NZERO
		"z"
		#else
		""
		#endif
		);
	fd=fopen(fnc,"w");
	for(i=1;i<XD*YD;i++,m++){
		if(*m<vmin) vmin=*m;
		if(*m>vmax) vmax=*m;
		if((*m!=ZERO)==t
			#ifdef NMAX32K
			&& n<32768
			#endif
			) n++;
		else{ write(n,t); t=(*m!=ZERO); n=1; }
	}
	write(n,t);
	fclose(fd);
	snprintf(fn,16,"mapf%i",DIV);
	fd=fopen(fn,"w");
	fwrite(map,sizeof(short),XD*YD,fd);
	fclose(fd);
	{
		int nbit=(int)ceil(log2(nmax));
		float nmem=nbit*nnum/8.f/1024.f;
		float vmem=12*vnum/8.f/1024.f;
		printf("vmin %7i\n",vmin);
		printf("vmax %7i\n",vmax);
		printf("nmax %7i -> bit: %i\n",nmax,nbit);
		printf("nnum %7i -> mem: %.0f kB\n",nnum,nmem);
		printf("vnum %7i -> mem: %.0f kB\n",vnum,vmem);
		printf("     %7s -> mem: %.0f kB\n","",nmem+vmem);
	}
	test(fnc);
	return 0;
}
