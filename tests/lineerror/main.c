#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL/SDL.h>
#include <GL/gl.h>
#include <math.h>

void paint(){
	float time=(float)SDL_GetTicks();
	int i;
	time/=5000.f;
	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DITHER);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.,1.,-1.,1.,-1.,1.);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glColor4f(1.f,1.f,1.f,1.f);
	glBegin(GL_QUADS);
	for(i=0;i<10;i++){
		time+=0.1f;
		time-=floor(time);
		glVertex2f(time*4.f-2.0f,-1.f);
		glVertex2f(time*4.f-1.9f,-1.f);
		glVertex2f(time*4.f-1.9f,+1.f);
		glVertex2f(time*4.f-2.0f,+1.f);
	}
	glEnd();
}

void readframe(){
	char buf[1024*768*3];
	int x,y;
	unsigned int hash;
	unsigned int hashdef;
	FILE *fd;
	glReadPixels(0,0,1024,768,GL_RGB,GL_UNSIGNED_BYTE,buf);
	fd=fopen("test.raw","w");
	fwrite(buf,3,1024*768,fd);
	fclose(fd);
	for(y=0;y<768;y++){
		hash=0;
		for(x=0;x<1024;x++) hash+=buf[(y*1024+x)*3]<<(x%4);
		if(!y) hashdef=hash;
		else if(hashdef!=hash){
			printf("Frame failed in line %i\n",y);
			return;
		}
	}
}

int main(int argc,char **argv){
	char quit=0;
	SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,4);
	SDL_SetVideoMode(1024,768,16,SDL_OPENGL|SDL_RESIZABLE);
	glViewport(0,0,1024,768);

	while(!quit){
		SDL_Event ev;
		if(SDL_PollEvent(&ev)) switch(ev.type){
		case SDL_KEYDOWN: if(ev.key.keysym.sym!=SDLK_q) break;
		case SDL_QUIT: quit=1; break;
		}
		paint();
		SDL_GL_SwapBuffers();
	}
	//readframe();
}
