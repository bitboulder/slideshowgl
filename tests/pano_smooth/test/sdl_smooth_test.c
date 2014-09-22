#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <SDL.h>
#include <GL/gl.h>

int time=20;
int sync=1;

void init(){
	SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,sync);
	SDL_SetVideoMode(1440,900,16,SDL_OPENGL|SDL_FULLSCREEN);
	SDL_SetVideoMode(400,300,16,SDL_OPENGL|SDL_RESIZABLE);
	SDL_WM_SetCaption("sdl_smooth","sdl_smooth");
	SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL,&sync);
	fprintf(stderr,"sync: %i\n",sync);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-0.5,0.5,0.5,-0.5,-1.,1.);
	glMatrixMode(GL_MODELVIEW);
}

void paint(){
	float rot=(float)SDL_GetTicks()/1000.f*M_PI;
	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glScalef(.4f,.4f,1.f);
	glTranslatef(cos(rot),sin(rot),0.f);
	glScalef(.5f,.5f,1.f);
	glColor4f(1.f,1.f,1.f,1.f);
	glRectf(-0.5f,-0.5f,0.5f,0.5f);
}

void loop(){
	Uint32 end=SDL_GetTicks()+1000*time;
	struct timeval tv[2];
	gettimeofday(tv,NULL);
	while(SDL_GetTicks()<end){
		long delta;
		paint();
		SDL_GL_SwapBuffers();
		gettimeofday(tv+1,NULL);
		delta=tv[1].tv_sec-tv[0].tv_sec;
		delta*=1000000;
		delta+=tv[1].tv_usec-tv[0].tv_usec;
		printf("%li\n",delta);
		tv[0]=tv[1];
		if(sync!=1) SDL_Delay(16);
	}
}

int main(){
	init();
	loop();
	SDL_Quit();
}
