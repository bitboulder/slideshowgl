#include <GL/gl.h>
#include <SDL.h>
#include "config.h"
#include "gl.h"
#include "main.h"
#include "sdl.h"

void glinit(){
}

enum glproject { GLP_3D, GLP_2D };
void glproject(enum glproject dst){
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	switch(dst){
	case GLP_3D: glFrustum(-1.0, 1.0, -sdl.scr_h, sdl.scr_h, 5.0, 60.0); break;
	case GLP_2D: glOrtho(0.,1.,1.,0.,-1.,1.); break;
	}
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	if(dst==GLP_3D) glTranslatef(0.0, 0.0, -40.0);
}

void glreshape(){
	glViewport(0, 0, (GLint)sdl.scr_w, (GLint)sdl.scr_h);
}

void glpaint(){
	glClearColor(0.,0.,0.,1.);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glproject(GLP_2D);

	glColor4f(1.,1.,1.,1.);
	glBegin(GL_QUADS);
	glVertex2f(.2,.2);
	glVertex2f(.6,.4);
	glVertex2f(.7,.7);
	glVertex2f(.3,.7);
	glEnd();
	
	SDL_GL_SwapBuffers();
}

