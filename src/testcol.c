#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const float cols[19][6]={
	{ 1.000f, 1.000f, 1.000f, 0.000f, 0.000f, 1.000f },
	{ 0.500f, 0.500f, 0.500f, 0.000f, 0.000f, 0.500f },
	{ 0.000f, 0.000f, 0.000f, 0.000f, 0.000f, 0.000f },
	{ 1.000f, 0.000f, 0.000f, 0.000f, 1.000f, 0.500f },
	{ 0.750f, 0.750f, 0.000f, 0.167f, 1.000f, 0.375f },
	{ 0.000f, 0.500f, 0.000f, 0.333f, 1.000f, 0.250f },
	{ 0.500f, 1.000f, 1.000f, 0.500f, 1.000f, 0.750f },
	{ 0.500f, 0.500f, 1.000f, 0.667f, 1.000f, 0.750f },
	{ 0.750f, 0.250f, 0.750f, 0.833f, 0.500f, 0.500f },
	{ 0.628f, 0.643f, 0.142f, 0.172f, 0.638f, 0.393f },
	{ 0.255f, 0.104f, 0.918f, 0.698f, 0.832f, 0.511f },
	{ 0.116f, 0.675f, 0.255f, 0.375f, 0.707f, 0.396f },
	{ 0.941f, 0.785f, 0.053f, 0.138f, 0.893f, 0.497f },
	{ 0.704f, 0.187f, 0.897f, 0.788f, 0.775f, 0.542f },
	{ 0.931f, 0.463f, 0.316f, 0.040f, 0.817f, 0.624f },
	{ 0.998f, 0.974f, 0.532f, 0.158f, 0.991f, 0.765f },
	{ 0.099f, 0.795f, 0.591f, 0.451f, 0.779f, 0.447f },
	{ 0.211f, 0.149f, 0.597f, 0.690f, 0.601f, 0.373f },
	{ 0.495f, 0.493f, 0.721f, 0.668f, 0.290f, 0.607f },
};

#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX(a,b)	((a)>(b)?(a):(b))

void col_hsl2rgb(float *dst,float *src){
	float c=(1.f-fabsf(2.f*src[2]-1.f))*src[1];
	float h=src[0]*6;
	float h2=h-truncf(h/2.f)*2.f;
	float x=c*(1.f-fabsf(h2-1.f));
	float m=src[2]-.5f*c;
	     if(h<1){ dst[0]=c; dst[1]=x; dst[2]=0; }
	else if(h<2){ dst[0]=x; dst[1]=c; dst[2]=0; }
	else if(h<3){ dst[0]=0; dst[1]=c; dst[2]=x; }
	else if(h<4){ dst[0]=0; dst[1]=x; dst[2]=c; }
	else if(h<5){ dst[0]=x; dst[1]=0; dst[2]=c; }
	else        { dst[0]=c; dst[1]=0; dst[2]=x; }
	dst[0]+=m;
	dst[1]+=m;
	dst[2]+=m;
	dst[3]=src[3];
}

void col_rgb2hsl(float *dst,float *src){
	float M=MAX(src[0],MAX(src[1],src[2]));
	float m=MIN(src[0],MIN(src[1],src[2]));
	float c=M-m;
		 if(c==0) dst[0]=0.f;
	else if(M==src[0]) dst[0]=(src[1]-src[2])/c;
	else if(M==src[1]) dst[0]=(src[2]-src[0])/c+2.f;
	else if(M==src[2]) dst[0]=(src[0]-src[1])/c+4.f;
	else dst[0]=0.f;
	dst[0]/=6.f; dst[0]-=truncf(dst[0]); if(dst[0]<0.f) dst[0]+=1.f;
	dst[2]=(M+m)*0.5f;
	if(c==0) dst[1]=0.f;
	else dst[1]=c/(1.f-fabsf(2*dst[2]-1.f));
}

int main(){
	int c,i;
	float src[4];
	float dst[4];
	src[3]=1.f;
	for(c=0;c<19;c++){
		char err=0;
		memcpy(src,cols[c],sizeof(float)*3);
		col_rgb2hsl(dst,src);
		for(i=0;i<3;i++) if(fabsf(dst[i]-cols[c][i+3])>0.004f) err=1;
		if(err) printf("rgb2hsl %2i: %5.3f %5.3f %5.3f => %5.3f %5.3f %5.3f (%5.3f %5.3f %5.3f)\n",c,
				src[0],src[1],src[2],dst[0],dst[1],dst[2],
				cols[c][3],cols[c][4],cols[c][5]);
		err=0;
		memcpy(src,cols[c]+3,sizeof(float)*3);
		col_hsl2rgb(dst,src);
		for(i=0;i<3;i++) if(fabsf(dst[i]-cols[c][i])>0.004f) err=1;
		if(err) printf("hsl2rgb %2i: %5.3f %5.3f %5.3f => %5.3f %5.3f %5.3f (%5.3f %5.3f %5.3f)\n",c,
				src[0],src[1],src[2],dst[0],dst[1],dst[2],
				cols[c][0],cols[c][1],cols[c][2]);
	}
}
