#include "config.h"
#include <SDL.h>
#include <SDL_thread.h>
#if HAVE_X11
	#include <SDL_syswm.h>
#endif
#if HAVE_X11 && HAVE_XEXT
	#include <X11/Xlib.h>
	#include <X11/extensions/dpms.h>
#endif
#if HAVE_X11 && HAVE_XINERAMA
	#include <X11/Xlib.h>
	#include <X11/extensions/Xinerama.h>
#endif
#include "sdl.h"
#include "gl.h"
#include "main.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"
#include "pano.h"
#include "eff.h"

char sdl_quit = 0;

SDL_Surface *screen;

struct sdlcfg {
	Uint32 hidecursor;
};

struct sdlmove {
	Uint16 base_x, base_y;
	int pos_x, pos_y;
	Uint32 time;
	Uint8 btn;
};

struct sdl {
	struct sdlcfg cfg;
	int scr_w, scr_h;
	int scrnof_w, scrnof_h;
	Uint32 hidecursor;
	struct sdlmove move;
	char fullscreen;
	char doresize;
	char sync;
} sdl = {
	.fullscreen = 0,
	.scr_w      = 0,
	.scr_h      = 0,
	.doresize   = 0,
	.sync       = 0,
	.hidecursor = 0,
	.move.base_x= 0xffff,
};

/* thread: gl */
float sdlrat(){ return (float)sdl.scr_w/(float)sdl.scr_h; }

#if HAVE_X11
Display *x11getdisplay(){
	SDL_SysWMinfo info;
	if(SDL_GetWMInfo(&info)!=1) return NULL;
	if(info.subsystem!=SDL_SYSWM_X11) return NULL;
	return info.info.x11.display;
}
#endif

#if HAVE_X11 && HAVE_XEXT
void switchdpms(char val){
	static BOOL state=1;
	int evb,erb;
	CARD16 plv;
	Display *display=x11getdisplay();
	if(!display || !DPMSQueryExtension(display,&evb,&erb) || !DPMSCapable(display)) return;
	if(!val){
		DPMSInfo(display,&plv,&state);
		DPMSDisable(display);
	}else if(state) DPMSEnable(display); else DPMSDisable(display);
}
#else
void switchdpms(char UNUSED(val)){ }
#endif

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
#if HAVE_X11 && HAVE_XINERAMA
{
	Display *display=x11getdisplay();
	XineramaScreenInfo *info;
	int ninfo,i;
	if(display && XineramaIsActive(display) && (info=XineramaQueryScreens(display,&ninfo))){
		*w=*h=0;
		for(i=0;i<ninfo;i++){
			//info[i].screen_number
			if(info[i].x_org+info[i].width> *w) *w=info[i].x_org+info[i].width;
			if(info[i].y_org+info[i].height>*w) *w=info[i].y_org+info[i].height;
		}
		free(info);
		if(*w && *h) return 1;
	}
}
#endif
{
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
}
	
void sdlresize(int w,int h){
	static char done=0;
	Uint32 flags=SDL_OPENGL;
	const SDL_VideoInfo *vi;
	GLenum glerr;
	sdl.doresize=0;
	if(done>=0){
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
		SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL,sdl.sync);
	}
	if(sdl.fullscreen && sdlgetfullscreenmode(flags|SDL_FULLSCREEN,&w,&h)){
		// TODO: Xinerama -> current screen
		debug(DBG_STA,"sdl set video mode fullscreen %ix%i",w,h);
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
	if((glerr=glGetError())) error(ERR_CONT,"in sdl view mode (gl-err: %d)",glerr);
	glViewport(0, 0, (GLint)sdl.scr_w, (GLint)sdl.scr_h);
	if(done>=0){
		int sync;
		SDL_WM_SetCaption("Slideshowgl","slideshowgl");
		SDL_GL_GetAttribute(SDL_GL_SWAP_CONTROL,&sync);
		if(sync!=1) sdl.sync=0;
		glinit(done);
		panoinit(done);
		if(done>0) ldreset();
		debug(DBG_STA,"sdl init (%ssync)",sdl.sync?"":"no");
		if((glerr=glGetError())) error(ERR_CONT,"in sdlinit (gl-err: %d)",glerr);
	}
	effrefresh(EFFREF_FIT);
#ifdef __WIN32__
	done=1;
#else
	done=-1;
#endif
}

void sdlinit(){
	sdl.sync=cfggetbool("sdl.sync");
	sdl.fullscreen=cfggetbool("sdl.fullscreen");
	sdl.cfg.hidecursor=cfggetuint("sdl.hidecursor");
	sdl.scrnof_w=cfggetint("sdl.width");
	sdl.scrnof_h=cfggetint("sdl.height");
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(ERR_QUIT,"sdl init failed");
	if(cfggetint("cfg.version")){
		const SDL_version* v = SDL_Linked_Version();
		char buf[32];
		mprintf("SDL-Version: %i.%i.%i (video: %s)\n",v->major,v->minor,v->patch,
				SDL_VideoDriverName(buf,32)?buf:_("(unknown)"));
	}
	sdlresize(sdl.scrnof_w,sdl.scrnof_h);
}

void sdlquit(){
	ldtexload();
	imgfinalize();
	glfree();
	SDL_Quit();
}

void sdlkey(SDL_keysym key){
	switch(key.sym){
		case SDLK_RIGHT:    dplevput(DE_RIGHT);   break;
		case SDLK_LEFT:     dplevput(DE_LEFT);    break;
		case SDLK_UP:       dplevput(DE_UP);      break;
		case SDLK_DOWN:     dplevput(DE_DOWN);    break;
		case SDLK_PAGEUP:   dplevput(DE_ZOOMIN);  break;
		case SDLK_PAGEDOWN: dplevput(DE_ZOOMOUT); break;
		case SDLK_r:        dplevput((key.mod&(KMOD_LSHIFT|KMOD_RSHIFT))?DE_ROT2:DE_ROT1); break;
		case SDLK_KP_ENTER:
		case SDLK_SPACE:    dplevput(DE_PLAY);    break;
		default:            dplevputk(key.sym);   break;
	}
}

void sdljump(Uint16 x,Uint16 y){
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
		if(ev) dplevput(ev);
	}else{
		if(xd || yd)
			dplevputp(DE_JUMP,
				-(float)xd/(float)sdl.scr_w,
				-(float)yd/(float)sdl.scr_h);
		sdl.move.base_x=x;
		sdl.move.base_y=y;
	}
}

void sdlclick(Uint8 btn,Uint16 x,Uint16 y){
	int zoom=dplgetzoom();
	float sx=(float)x/(float)sdl.scr_w-.5f;
	float sy=(float)y/(float)sdl.scr_h-.5f;
	if(btn==SDL_BUTTON_MIDDLE){
		if(dplgetpos()->writemode) dplevputp(DE_MARK,sx,sy);
		else dplevputp(DE_PLAY,sx,sy);
	}else if(zoom==0) switch(btn){
		case SDL_BUTTON_LEFT:  dplevput(DE_RIGHT); break;
		case SDL_BUTTON_RIGHT: dplevput(DE_LEFT); break;
	}else if(zoom<0 && btn==SDL_BUTTON_LEFT){
		dplevputp(DE_SEL,sx,sy);
	}else if(zoom>0 && btn==SDL_BUTTON_LEFT){
		dplevputp(DE_MOVE,sx,sy);
	}
}

void sdlmotion(Uint16 x,Uint16 y){
	SDL_ShowCursor(SDL_ENABLE);
	sdl.hidecursor=SDL_GetTicks()+sdl.cfg.hidecursor;
	//printixy((float)x/(float)sdl.scr_w-.5f,(float)y/(float)sdl.scr_h-.5f);
	if(sdl.move.base_x!=0xffff) sdljump(x,y);
	else dplevput(DE_STAT);
}

void sdlbutton(char down,Uint8 button,Uint16 x,Uint16 y){
	float fx = (float)x/(float)sdl.scr_w - .5f;
	float fy = (float)y/(float)sdl.scr_h - .5f;
	if(down){
		switch(button){
		case SDL_BUTTON_LEFT:
		case SDL_BUTTON_MIDDLE:
		case SDL_BUTTON_RIGHT:
			sdl.move.btn=button;
			sdl.move.time=SDL_GetTicks();
		}
	}else if(button==sdl.move.btn && SDL_GetTicks()-sdl.move.time<200){
		if(button==SDL_BUTTON_LEFT) sdl.move.base_x=0xffff;
		sdlclick(button,x,y);
		return;
	}
	if(down && button==SDL_BUTTON_WHEELUP) dplevputp(DE_ZOOMIN,fx,fy);
	else if(down && button==SDL_BUTTON_WHEELDOWN) dplevputp(DE_ZOOMOUT,fx,fy);
	else if(down && button==SDL_BUTTON_LEFT){
		sdl.move.pos_x=0;
		sdl.move.pos_y=0;
		sdl.move.base_x=x;
		sdl.move.base_y=y;
	}else if(!down && button==SDL_BUTTON_LEFT){
		sdljump(x,y);
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
	Uint32 diff=SDL_GetTicks()-*last;
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
		float fr = (float)n/(float)(t_now-t_last)*1000.f;
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

int sdlthread(void *UNUSED(arg)){
	switchdpms(0);
	while(!sdl_quit){
		if(!sdlgetevent()) break;
		if(sdl.doresize) sdlresize(0,0);
		sdlhidecursor();
		timer(TI_SDL,0,1);
		
		if(!effineff()) ldtexload();
		while(SDL_GetTicks()-paint_last < (effineff()?6:20)) if(!ldtexload()) break;
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
	sdl_quit|=THR_SDL;
	return 0;
}

