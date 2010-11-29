#include "config.h"
#include <SDL.h>
#include <SDL_thread.h>
#if HAVE_X11 && HAVE_XEXT
	#include <X11/Xlib.h>
	#include <X11/extensions/dpms.h>
#endif
#include "sdl.h"
#include "gl.h"
#include "main.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"
#include "pano.h"

SDL_Surface *screen;

struct sdl sdl = {
	.quit       = 0,
	.fullscreen = 0,
	.scr_w      = 0,
	.scr_h      = 0,
	.doresize   = 0,
	.sync       = 0,
	.writemode  = 0,
	.hidecursor = 0,
	.move.base_x= 0xffff,
};

void switchdpms(char val){
#if HAVE_X11 && HAVE_XEXT
	static BOOL state=1;
	int evb,erb;
	CARD16 plv;
	Display *display=XOpenDisplay(NULL);
	if(!display || !DPMSQueryExtension(display,&evb,&erb) || !DPMSCapable(display)) return;
	if(!val){
		DPMSInfo(display,&plv,&state);
		DPMSDisable(display);
	}else if(state) DPMSEnable(display); else DPMSDisable(display);
	XCloseDisplay(display);
#endif
}

/* thread: dpl */
void sdlfullscreen(){
	if(sdl.fullscreen){
		sdl.scr_w=sdl.scrnof_w;
		sdl.scr_h=sdl.scrnof_h;
		sdl.fullscreen=0;
	}else{
		sdl.scrnof_w=sdl.scr_w;
		sdl.scrnof_h=sdl.scr_h;
		sdl.fullscreen=1;
	}
	sdl.doresize=1;
}

char sdlgetfullscreenmode(Uint32 flags,int *w,int *h){
	SDL_Rect** modes=SDL_ListModes(SDL_GetVideoInfo()->vfmt,flags);
	if(modes==(SDL_Rect**)-1) error(ERR_CONT,"sdl All fullscreen modes available");
	else if(modes==(SDL_Rect**)0 || !modes[0]) error(ERR_CONT,"sdl No fullscreen modes available");
	else{
		int i;
		*w=*h=0;
		for(i=0;modes[i];i++) if(modes[i]->w>*w){ *w=modes[i]->w; *h=modes[i]->h; }
	}
	return *w && *h;
}
	
void sdlresize(int w,int h){
	Uint32 flags=SDL_OPENGL|SDL_DOUBLEBUF;
	const SDL_VideoInfo *vi;
	sdl.doresize=0;
	if(sdl.fullscreen && sdlgetfullscreenmode(flags|SDL_FULLSCREEN,&w,&h)){
		debug(DBG_STA,"sdl set video mode fullscreen");
		if(!(screen=SDL_SetVideoMode(w,h,16,flags|SDL_FULLSCREEN))) error(ERR_QUIT,"video mode init failed");
	}else{
		if(!w) w=sdl.scr_w;
		if(!h) h=sdl.scr_h;
		debug(DBG_STA,"sdl set video mode %ix%i",sdl.scr_w,sdl.scr_h);
		if(!(screen=SDL_SetVideoMode(w,h,16,flags|SDL_RESIZABLE))) error(ERR_QUIT,"video mode init failed");
	}
	vi=SDL_GetVideoInfo();
	sdl.scr_w=vi->current_w;
	sdl.scr_h=vi->current_h;
	debug(DBG_STA,"sdl get video mode %ix%i",sdl.scr_w,sdl.scr_h);
	glreshape();
	effrefresh(EFFREF_FIT);
}

void sdlinit(){
	int sync;
	sdl.sync=cfggetint("sdl.sync");
	sdl.fullscreen=cfggetint("sdl.fullscreen");
	sdl.cfg.hidecursor=cfggetint("sdl.hidecursor");
	sdl.scrnof_w=cfggetint("sdl.width");
	sdl.scrnof_h=cfggetint("sdl.height");
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(ERR_QUIT,"sdl init failed");
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,sdl.sync);
	sdlresize(sdl.scrnof_w,sdl.scrnof_h);
	SDL_WM_SetCaption("Slideshowgl","slideshowgl");
	SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL,&sync);
	if(sync!=1) sdl.sync=0;
	glinit();
	panoinit();
	debug(DBG_STA,"sdl init (%ssync)",sdl.sync?"":"no");
}

void sdlquit(){
	ldtexload();
	imgfinalize();
	glfree();
	SDL_Quit();
}

void sdlkey(SDL_keysym key){
	switch(key.sym){
		case SDLK_RIGHT:    dplevput(DE_RIGHT  ,key.sym); break;
		case SDLK_LEFT:     dplevput(DE_LEFT   ,key.sym); break;
		case SDLK_UP:       dplevput(DE_UP     ,key.sym); break;
		case SDLK_DOWN:     dplevput(DE_DOWN   ,key.sym); break;
		case SDLK_PAGEUP:   dplevput(DE_ZOOMIN ,key.sym); break;
		case SDLK_PAGEDOWN: dplevput(DE_ZOOMOUT,key.sym); break;
		case SDLK_r:        dplevput((key.mod&(KMOD_LSHIFT|KMOD_RSHIFT))?DE_ROT2:DE_ROT1,key.sym); break;
		case SDLK_KP_ENTER:
		case SDLK_SPACE:    dplevput(DE_PLAY   ,key.sym); break;
		default:            dplevput(DE_KEY    ,key.sym); break;
	}
}

void sdlmove(Uint16 x,Uint16 y){
	int xd=x-sdl.move.base_x, yd=y-sdl.move.base_y;
	int zoom=dplgetzoom();
	int w=100,wthr;
	enum dplev ev=0;
	if(zoom<=0){
		switch(zoom){
			case -1: w=sdl.scr_h/3/2; break;
			case -2: w=sdl.scr_h/5/2; break;
			case -3: w=sdl.scr_h/7/2; break;
		}
		if(zoom!=0){ xd*=-1; yd*=-1; }
		wthr=w*7/10;
		if(xd<sdl.move.pos_x*w-wthr){ ev=DE_LEFT;  sdl.move.pos_x--; }
		if(xd>sdl.move.pos_x*w+wthr){ ev=DE_RIGHT; sdl.move.pos_x++; }
		if(yd<sdl.move.pos_y*w-wthr){ ev=DE_UP;    sdl.move.pos_y--; }
		if(yd>sdl.move.pos_y*w+wthr){ ev=DE_DOWN;  sdl.move.pos_y++; }
		if(ev) dplevput(ev,0);
	}else{
		dplevputx(DE_MOVE,0,
			-(float)xd/(float)sdl.scr_w,
			-(float)yd/(float)sdl.scr_h);
		sdl.move.base_x=x;
		sdl.move.base_y=y;
	}
}

void sdlmotion(Uint16 x,Uint16 y){
	SDL_ShowCursor(SDL_ENABLE);
	sdl.hidecursor=SDL_GetTicks()+sdl.cfg.hidecursor;
	//printixy((float)x/(float)sdl.scr_w-.5f,(float)y/(float)sdl.scr_h-.5f);
	if(sdl.move.base_x!=0xffff) sdlmove(x,y);
	else dplevput(DE_STAT,0);
}

void sdlbutton(char down,Uint8 button,Uint16 x,Uint16 y){
	float fx = (float)x/(float)sdl.scr_w - .5f;
	float fy = (float)y/(float)sdl.scr_h - .5f;
	if(down && button==SDL_BUTTON_WHEELUP) dplevputx(DE_ZOOMIN,0,fx,fy);
	else if(down && button==SDL_BUTTON_WHEELDOWN) dplevputx(DE_ZOOMOUT,0,fx,fy);
	else if(down && button==SDL_BUTTON_LEFT){
		sdl.move.pos_x=0;
		sdl.move.pos_y=0;
		sdl.move.base_x=x;
		sdl.move.base_y=y;
	}else if(!down && button==SDL_BUTTON_LEFT){
		sdlmove(x,y);
		sdl.move.base_x=0xffff;
	}
}

void sdlhidecursor(){
	if(!sdl.hidecursor || SDL_GetTicks()<sdl.hidecursor) return;
	SDL_ShowCursor(SDL_DISABLE);
	sdl.hidecursor=0;
}

char sdlgetevent(){
	SDL_Event ev;
	if(!SDL_PollEvent(&ev)) return 1;
	switch(ev.type){
	case SDL_VIDEORESIZE: sdlresize(ev.resize.w,ev.resize.h); break;
	case SDL_KEYDOWN: sdlkey(ev.key.keysym); break;
	case SDL_MOUSEMOTION: sdlmotion(ev.motion.x,ev.motion.y); break;
	case SDL_MOUSEBUTTONDOWN: sdlbutton(1,ev.button.button,ev.button.x,ev.button.y); break;
	case SDL_MOUSEBUTTONUP:   sdlbutton(0,ev.button.button,ev.button.x,ev.button.y); break;
	case SDL_QUIT: return 0;
	}
	return 1;
}

/* thread: sdl, dpl */
void sdldelay(Uint32 *last,Uint32 delay){
	int diff=SDL_GetTicks()-*last;
	if(diff<delay) SDL_Delay(delay-diff);
	*last=SDL_GetTicks();
}

void sdlframerate(){
	static Uint32 t_last = 0;
	static Uint32 n = 0;
	Uint32 t_now = SDL_GetTicks();
	if(!t_last) t_last=t_now;
	else n++;
	if(t_now-t_last>1000){
		float fr = n/(float)(t_now-t_last)*1000.;
		debug(DBG_STA,"sdl framerate %.1f fps",fr);
		t_last=t_now;
		n=0;
	}
}

	Uint32 paint_last=0;
	/* thread: dpl, load */
	/* TODO: remove */
	void sdlthreadcheck(){
		if(SDL_GetTicks()-paint_last>10000)
			system("killall -9 slideshowgl");
	}

int sdlthread(void *arg){
	switchdpms(0);
	while(!sdl.quit){
		if(!sdlgetevent()) break;
		if(sdl.doresize) sdlresize(0,0);
		sdlhidecursor();
		timer(TI_SDL,0,1);
		
		if(!dplineff()) ldtexload();
		while(SDL_GetTicks()-paint_last < (dplineff()?6:20)) if(!ldtexload()) break;
		timer(TI_SDL,1,1);

		glpaint();
		timer(TI_SDL,2,1);

		SDL_GL_SwapBuffers();
		timer(TI_SDL,3,1);


		sdlframerate();
		if(!sdl.sync) sdldelay(&paint_last,16);
		else paint_last=SDL_GetTicks(); /* TODO remove (for sdlthreadcheck) */
		timer(TI_SDL,4,1);
	}
	switchdpms(1);
	sdl.quit|=THR_SDL;
	return 0;
}

