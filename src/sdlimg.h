#ifndef _SDLIMG_H
#define _SDLIMG_H

#include <GL/glew.h>

struct sdlimg {
	SDL_Surface *sf;
	GLenum fmt;
	int ref;
};

void sdlimg_unref(struct sdlimg *sdlimg);
void sdlimg_ref(struct sdlimg *sdlimg);
struct sdlimg* sdlimg_gen(SDL_Surface *sf);

#endif
