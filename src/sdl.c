#include "config.h"
#ifndef __WIN32__
	#define _POSIX_C_SOURCE 200112L
	#include <features.h>
#endif
#include <SDL.h>
#include <SDL_image.h>
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
#include "gl_int.h"
#include "main.h"
#include "load.h"
#include "dpl.h"
#include "cfg.h"
#include "pano.h"
#include "eff.h"
#include "help.h"
#include "ldjpg.h"
#include "map.h"
#include "il.h"

char sdl_quit = 0;

struct sdlcfg {
	Uint32 hidecursor;
	Uint32 doubleclicktime;
	int fsaa;
	int fsaamaxw;
	int fsaamaxh;
	const char *playrecord;
	int display;
	char envdisplay;
};

struct sdlmove {
	Sint32 base_x, base_y;
	int pos_x, pos_y;
	int clickimg;
	unsigned short mid;
	char jump;
};

struct sdl {
	struct sdlcfg cfg;
	int scr_w, scr_h;
	int off_y;
	Uint32 hidecursor;
	struct sdlmove move;
	Sint32 mousex,mousey;
	char fullscreen;
	char dofullscreen;
	char sync;
	SDL_Window *wnd;
	struct sdlclickdelay {
		Uint32 time;
		Uint8 btn;
		Sint32 x,y;
		int clickimg;
	} clickdelay;
	Uint32 lastfrm;
} sdl = {
	.fullscreen = 0,
	.scr_w      = 0,
	.scr_h      = 0,
	.off_y      = 0,
	.dofullscreen= 0,
	.sync       = 0,
	.hidecursor = 0,
	.move.base_x= 0xffff,
	.mousex     = 0,
	.mousey     = 0,
	.clickdelay.time = 0,
	.lastfrm    = 0,
};

/* thread: gl */
float sdlrat(){ return (float)sdl.scr_w/(float)sdl.scr_h; }

/* thread: dpl */
void sdlwh(float *w,float *h){
	if(w) *w=(float)sdl.scr_w;
	if(h) *h=(float)sdl.scr_h;
}

void sdlforceredraw(){
	sdl.lastfrm=0;
}

#if HAVE_X11 && HAVE_XEXT
void switchdpms(char val){
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
}
#else
void switchdpms(char UNUSED(val)){ }
#endif

/* thread: dpl */
char sdlfullscreen(char dst){
	if(dst==sdl.fullscreen) return 0;
	sdl.fullscreen=!sdl.fullscreen;
	sdl.dofullscreen=1;
	return 1;
}

struct subdpl {
	char set;
	int x,y,w,h;
};

/*#if HAVE_X11 && HAVE_XINERAMA
char sdlgetfullscreenmode(Uint32 flags,int *w,int *h,struct subdpl *subdpl)
#else
char sdlgetfullscreenmode(Uint32 flags,int *w,int *h,struct subdpl *UNUSED(subdpl))
#endif
{
#if HAVE_X11 && HAVE_XINERAMA
{
	Display *display=XOpenDisplay(NULL);
	XineramaScreenInfo *info;
	int ninfo,i;
	if(display && XineramaIsActive(display) && (info=XineramaQueryScreens(display,&ninfo))){
		*w=*h=0;
		for(i=0;i<ninfo;i++){
			//info[i].screen_number
			if(info[i].x_org+info[i].width> *w) *w=info[i].x_org+info[i].width;
			if(info[i].y_org+info[i].height>*h) *h=info[i].y_org+info[i].height;
			if(info[i].screen_number==sdl.cfg.display){
				subdpl->set=1;
				subdpl->x=info[i].x_org;
				subdpl->y=info[i].y_org;
				subdpl->w=info[i].width;
				subdpl->h=info[i].height;
			}
		}
		free(info);
		if(*w && *h) return 1;
	}
	XCloseDisplay(display);
}
#endif
{*/
/*	SDL_Rect** modes=SDL_ListModes(SDL_GetVideoInfo()->vfmt,flags); TODO: check
	if(modes==(SDL_Rect**)-1) error(ERR_CONT,"sdl All fullscreen modes available");
	else if(modes==(SDL_Rect**)0 || !modes[0]) error(ERR_CONT,"sdl No fullscreen modes available");
	else{
		int i;
		*w=*h=0;
		for(i=0;modes[i];i++) if(modes[i]->w>*w){ *w=modes[i]->w; *h=modes[i]->h; }
	}
	return *w && *h;*/
/*}
	return 0;
}*/
	
void sdlresize(int w,int h){
	int fsaa=sdl.cfg.fsaa;
	debug(DBG_STA,"sdl window resize %ix%i",w,h);
	sdl.scr_w=w; sdl.scr_h=h;
	if(fsaa && (w>sdl.cfg.fsaamaxw || h>sdl.cfg.fsaamaxh)){
		debug(DBG_STA,"disable anti-aliasing for window size");
		fsaa=0;
	}
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,fsaa?1:0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,fsaa);
	glViewport(0, 0, (GLint)sdl.scr_w, (GLint)sdl.scr_h);
	ileffref(CIL_ALL,EFFREF_FIT);
	sdlforceredraw();
}

void sdlinitfullscreen(){
	sdl.dofullscreen=0;
	if(sdl.fullscreen){
		SDL_SetWindowPosition(sdl.wnd,0,0); // TODO: set position according to desired display
		SDL_SetWindowFullscreen(sdl.wnd,SDL_WINDOW_FULLSCREEN_DESKTOP);
	}else{
		SDL_SetWindowFullscreen(sdl.wnd,0);
	}
/*#if HAVE_X11
	{ TODO
		Display *display=XOpenDisplay(NULL);
		XWrapPointer(display,0,sdl.wnd,0,0,0,0,sdl.scr_w/2,sdl.scr_h/2);
	}
#endif*/
}

void sdlinitwnd(){
	GLenum glerr;
	SDL_Renderer *rnd;
	if(SDL_CreateWindowAndRenderer(sdl.scr_w,sdl.scr_h,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE,&sdl.wnd,&rnd)<0) error(ERR_QUIT,"window creation failed");
	SDL_GetWindowSize(sdl.wnd,&sdl.scr_w,&sdl.scr_h);
	debug(DBG_STA,"sdl init window %ix%i",sdl.scr_w,sdl.scr_h);
	SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,"0");
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	if(sdl.sync && SDL_GL_SetSwapInterval(-1)<0 && SDL_GL_SetSwapInterval(1)<0){
		sdl.sync=0;
		error(ERR_CONT,"swap interval failed");
	}
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	sdlinitfullscreen();
	if((glerr=glGetError())) error(ERR_CONT,"in sdl window creation (gl-err: %d)",glerr);
	glinit(0);
	panoinit(0);
	if((glerr=glGetError())) error(ERR_CONT,"in glinit (gl-err: %d)",glerr);
}

void sdlicon(){
	const char *fn=finddatafile("icon.png");
	SDL_Surface *icon=IMG_Load(fn);
	if(!icon){ error(ERR_CONT,"icon load failed (%s)",fn); return; }
#ifdef __WIN32__
	if(icon->w!=32){
		SDL_Surface *sicon=SDL_ScaleSurfaceFactor(icon,icon->w/32,0,0,32,32,0);
		if(sicon){
			SDL_FreeSurface(icon);
			icon=sicon;
		}
	}
#endif
	SDL_SetWindowIcon(sdl.wnd,icon);
	SDL_FreeSurface(icon);
}

void sdlinit(){
	sdl.sync=cfggetbool("sdl.sync");
	sdl.fullscreen=cfggetbool("sdl.fullscreen");
	sdl.cfg.hidecursor=cfggetuint("sdl.hidecursor");
	sdl.scr_w=cfggetint("sdl.width");
	sdl.scr_h=cfggetint("sdl.height");
	sdl.cfg.doubleclicktime=cfggetuint("sdl.doubleclicktime");
	sdl.cfg.fsaa=cfggetint("sdl.fsaa");
	sdl.cfg.fsaamaxw=cfggetint("sdl.fsaa_max_w");
	sdl.cfg.fsaamaxh=cfggetint("sdl.fsaa_max_h");
	sdl.cfg.playrecord=cfggetstr("sdpl.playrecord");
	if(sdl.cfg.playrecord && sdl.cfg.playrecord[0]){
		sdl.scr_w=cfggetint("sdl.playrecord_w");
		sdl.scr_h=cfggetint("sdl.playrecord_h");
		sdl.fullscreen=0;
	}else sdl.cfg.playrecord=NULL;
	{
		char buf[8];
		snprintf(buf,8,"%i",sdl.cfg.display=cfggetint("sdl.display"));
#ifdef __WIN32__
		sdl.cfg.envdisplay=0;
#else
		sdl.cfg.envdisplay=!setenv("SDL_VIDEO_FULLSCREEN_DISPLAY",buf,1);
#endif
	}
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(ERR_QUIT,"sdl init failed");
	/*SDL_EnableUNICODE(1); TODO: check */
	if(cfggetint("cfg.version")){
		SDL_version v;
		SDL_GetVersion(&v);
		mprintf("SDL-Version: %i.%i.%i\n",v.major,v.minor,v.patch);
	}
	sdlinitwnd();
	sdlicon();
	sdlresize(sdl.scr_w,sdl.scr_h);
	mapsdlsize(&sdl.scr_w,&sdl.scr_h);
}

void sdlquit(){
	ldtexload();
	ilsfinalize();
	glfree();
	SDL_Quit();
}

unsigned short keytabn[]={
       /*  0    1    2    3    4    5    6    7     8    9    A    B    C    D    E    F */
/* 00 */    0,   0,   0,   0,   0,   0,   0,   0, '\b','\t',   0,   0,   0,'\r',   0,   0,
/* 10 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,0x1B,   0,   0,   0,   0,
/* 20 */  ' ',   0,   0, '#',   0,   0,   0,   0,    0,   0,   0, '+', ',', '-', '.',   0,
/* 30 */  '0', '1', '2', '3', '4', '5', '6', '7', '8',  '9',   0,   0, '<',   0,   0,   0,
/* 40 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* 50 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* 60 */    0, 'a', 'b', 'c', 'd', 'e', 'f', 'g',  'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
/* 70 */  'p', 'q', 'r', 's', 't', 'u', 'v', 'w',  'x', 'y', 'z',   0,   0,   0,   0,0x7F,
/* 80 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* 90 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* A0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* B0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* C0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* D0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* E0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* F0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
};

unsigned short keytabs[]={
       /*  0    1    2    3    4    5    6    7     8    9    A    B    C    D    E    F */
/* 00 */    0,   0,   0,   0,   0,   0,   0,   0, '\b','\t',   0,   0,   0,'\r',   0,   0,
/* 10 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,0x1B,   0,   0,   0,   0,
/* 20 */  ' ',   0,   0,'\'',   0,   0,   0,   0,    0,   0,   0, '*', ';', '_', ':',   0,
/* 30 */  '=', '!', '"',   0, '$', '%', '&', '/', '(',  ')',   0,   0, '>',   0,   0,   0,
/* 40 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* 50 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* 60 */    0, 'A', 'B', 'C', 'D', 'E', 'F', 'G',  'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
/* 70 */  'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',  'X', 'Y', 'Z',   0,   0,   0,   0,0x7F,
/* 80 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* 90 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* A0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* B0 */    0,   0,   0,   0, '`',   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* C0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* D0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0, '?',
/* E0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
/* F0 */    0,   0,   0,   0,   0,   0,   0,   0,    0,   0,   0,   0,   0,   0,   0,   0,
};

void sdlkey(SDL_Keysym key){
	switch(key.sym){
		case SDLK_RIGHT:    dplevput(DE_RIGHT);   break;
		case SDLK_LEFT:     dplevput(DE_LEFT);    break;
		case SDLK_UP:       dplevput(DE_UP);      break;
		case SDLK_DOWN:     dplevput(DE_DOWN);    break;
		case SDLK_PAGEUP:   dplevput(DE_ZOOMIN);  break;
		case SDLK_PAGEDOWN: dplevput(DE_ZOOMOUT); break;
		case SDLK_KP_ENTER: dplevputk(' ');       break;
		case SDLK_HOME:     dplevputi(DE_SEL,0); break;
		case SDLK_END:      dplevputi(DE_SEL,IMGI_END-1); break;
		default:
			if((key.mod&KMOD_SHIFT)){
				if(key.sym<(Sint32)sizeof(keytabs) && keytabs[key.sym]) dplevputk(keytabs[key.sym]);
			}else{
				if(key.sym<(Sint32)sizeof(keytabn) && keytabn[key.sym]) dplevputk(keytabn[key.sym]);
			}
		break;
	}
}

char sdljump(Sint32 x,Sint32 y,char end){
	int xd=x-sdl.move.base_x, yd=y-sdl.move.base_y;
	int zoom=dplgetzoom();
	int w=100,wthr;
	enum dplev ev=0;
	int actil=dplgetactil(NULL);
	if(sdl.move.base_x==0xffff) return 0;
	if(!sdl.move.jump && abs(x-sdl.move.base_x)<10 && abs(y-sdl.move.base_y)<10) return 0;
	sdl.move.jump=1;
	if(zoom<=0 && !mapon() && actil<1){
		switch(zoom){
			case  0: if(actil==0) w=sdl.scr_h/5/2; break;
			case -1: w=sdl.scr_h/3/2; break;
			case -2: w=sdl.scr_h/5/2; break;
			case -3: w=sdl.scr_h/7/2; break;
		}
		if(zoom!=0){ xd*=-1; yd*=-1; }
		else if(actil==0){ xd=0; yd*=-1; }
		wthr=w*7/10;
		if(xd<sdl.move.pos_x*w-wthr){ ev=DE_LEFT;  sdl.move.pos_x--; }
		if(xd>sdl.move.pos_x*w+wthr){ ev=DE_RIGHT; sdl.move.pos_x++; }
		if(yd<sdl.move.pos_y*w-wthr){ ev=DE_UP;    sdl.move.pos_y--; }
		if(yd>sdl.move.pos_y*w+wthr){ ev=DE_DOWN;  sdl.move.pos_y++; }
		if(ev) dplevputs(ev,DES_MOUSE);
	}else{
		if(xd || yd)
			dplevputx(DE_JUMP,
				sdl.move.mid,
				-(float)xd/(float)sdl.scr_w,
				-(float)yd/(float)sdl.scr_h,
				sdl.move.clickimg,
				DES_MOUSE);
		sdl.move.base_x=x;
		sdl.move.base_y=y;
		if(end) dplevputx(DE_JUMPEND,sdl.move.mid,0.f,0.f,sdl.move.clickimg,DES_MOUSE);
	}
	return 1;
}

void sdlclick(Uint8 btn,Sint32 x,Sint32 y,int clickimg){
	int zoom=dplgetzoom();
	float sx=(float)x/(float)sdl.scr_w-.5f;
	float sy=(float)y/(float)sdl.scr_h-.5f;
	char doubleclick;
	Uint32 now=SDL_GetTicks();
	if((doubleclick=(sdl.clickdelay.btn==btn && sdl.clickdelay.time>=now))){
		sdl.clickdelay.time=0;
		x=sdl.clickdelay.x;
		y=sdl.clickdelay.y;
		clickimg=sdl.clickdelay.clickimg;
	}else{
		sdl.clickdelay.time=now+sdl.cfg.doubleclicktime;
		sdl.clickdelay.btn=btn;
		sdl.clickdelay.x=x;
		sdl.clickdelay.y=y;
		sdl.clickdelay.clickimg=clickimg;
	}
	if(clickimg>=IMGI_COL) switch(btn){
		case SDL_BUTTON_LEFT:
			if(!doubleclick) dplevputi(DE_COL,clickimg-IMGI_COL);
		break;
	}else if(clickimg>=IMGI_CAT) switch(btn){
		case SDL_BUTTON_LEFT:
			dplevputi(DE_MARK,clickimg);
			if(doubleclick) dplevputi(DE_DIR,clickimg);
		break;
	}else if(clickimg>=IMGI_MAP) switch(btn){
		case SDL_BUTTON_LEFT:
			if(doubleclick) dplevputi(DE_DIR,clickimg);
		break;
		case SDL_BUTTON_RIGHT:
			if(mapgetcltpos(clickimg-IMGI_MAP,&sx,&sy)) dplevputp(DE_MAPMK,sx,sy);
		break;
	}else switch(btn){
		case SDL_BUTTON_LEFT:
			if(zoom>0 || mapon()) dplevputp(DE_MOVE,sx,sy);
			else if(doubleclick){
				if(zoom==0)       dplevputs(DE_LEFT,DES_MOUSE);
				dplevputi(DE_MOV|DE_DIR|DE_ZOOMIN,clickimg);
			}else if(zoom==0)     dplevputs(DE_RIGHT,DES_MOUSE);
			else                  dplevputi(DE_SEL,clickimg);
		break;
		case SDL_BUTTON_MIDDLE: if(!mapon()) dplevputi(DE_MARK|DE_STOP|DE_PLAY,clickimg); break;
		case SDL_BUTTON_RIGHT:
			if(mapon()) dplevputp(DE_MAPMK,sx,sy);
			else if(zoom==0) dplevputs(DE_LEFT,DES_MOUSE);
		break;
	}
}

void sdlmotion(Sint32 x,Sint32 y){
	float fx = (float)x/(float)sdl.scr_w - .5f;
	float fy = (float)y/(float)sdl.scr_h - .5f;
	sdl.mousex=x; sdl.mousey=y;
	SDL_ShowCursor(SDL_ENABLE);
	sdl.hidecursor=SDL_GetTicks()+sdl.cfg.hidecursor;
	//printixy((float)x/(float)sdl.scr_w-.5f,(float)y/(float)sdl.scr_h-.5f);
	if(sdl.move.base_x!=0xffff) sdljump(x,y,0);
	else if(dplgetactil(NULL)>=0 || mapon()) 
		dplevputx(DE_STAT,0,fx,fy,glselect(x,y+sdl.off_y),DES_MOUSE);
	else dplevputs(DE_STAT,DES_MOUSE);
}

void sdlbutton(char down,Uint8 button,Sint32 x,Sint32 y){
	int clickimg = glselect(x,y+sdl.off_y);
	if(down) switch(button){
		case SDL_BUTTON_RIGHT:
			if(clickimg<IMGI_MAP || clickimg>=IMGI_CAT) break;
		case SDL_BUTTON_LEFT:
			sdl.move.jump=0;
			sdl.move.pos_x=0;
			sdl.move.pos_y=0;
			sdl.move.base_x=x;
			sdl.move.base_y=y;
			sdl.move.mid = button==SDL_BUTTON_RIGHT ? 1 : 0;
			sdl.move.clickimg = clickimg;
		break;
	}else switch(button){
		case SDL_BUTTON_LEFT:
		case SDL_BUTTON_RIGHT:
			if(!sdljump(x,y,1)) sdlclick(button,x,y,clickimg);
			sdl.move.base_x=0xffff;
		break;
		case SDL_BUTTON_MIDDLE:    sdlclick(button,x,y,clickimg);         break;
	}
}

void sdlwheel(Sint32 wy){
	float fx = (float)sdl.mousex/(float)sdl.scr_w - .5f;
	float fy = (float)sdl.mousey/(float)sdl.scr_h - .5f;
	int clickimg = glselect(sdl.mousex,sdl.mousey+sdl.off_y);
	dplevputpi(wy<0?DE_ZOOMOUT:DE_ZOOMIN,fx,fy,clickimg);
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
	case SDL_WINDOWEVENT:
		switch(ev.window.event){
		case SDL_WINDOWEVENT_RESIZED: sdlresize(ev.window.data1,ev.window.data2); break;
		case SDL_WINDOWEVENT_EXPOSED: sdlforceredraw(); break;
		} break;
	case SDL_KEYDOWN: sdlkey(ev.key.keysym); break;
	case SDL_MOUSEMOTION: sdlmotion(ev.motion.x,ev.motion.y); break;
	case SDL_MOUSEBUTTONDOWN: sdlbutton(1,ev.button.button,ev.button.x,ev.button.y); break;
	case SDL_MOUSEBUTTONUP:   sdlbutton(0,ev.button.button,ev.button.x,ev.button.y); break;
	case SDL_MOUSEWHEEL:      sdlwheel(ev.wheel.y); break;
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

void sdlsaveframe(){
	static int w=0,h=0;
	static unsigned char *buf=NULL;
	char fn[FILELEN];
	if(w!=sdl.scr_w || h!=sdl.scr_h){
		w=sdl.scr_w;
		h=sdl.scr_h;
		buf=realloc(buf,(size_t)w*(size_t)h*3);
	}
	glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,buf);
	snprintf(fn,FILELEN,"%s_%06i.jpg",sdl.cfg.playrecord,dplgetfid());
	debug(DBG_STA,"sdl save frame %s",fn);
	jpegsave(fn,(unsigned int)w,(unsigned int)h,buf);
}

int sdlthread(void *UNUSED(arg)){
	Uint32 paint_last=0;
	switchdpms(0);
	while(!sdl_quit){
		if(!sdlgetevent()) break;
		if(sdl.dofullscreen) sdlinitfullscreen();
		sdlhidecursor();
		timer(TI_SDL,0,1);
		
		if(!effineff()) ldtexload();
		while(SDL_GetTicks()-paint_last < (effineff()?6:20)) if(!ldtexload()) break;

		timer(TI_SDL,1,1);
		if(sdl.lastfrm && sdl.lastfrm>efflastchg()){
			if(sdl.cfg.playrecord) sdlsaveframe();
			else sdldelay(&paint_last,10);
			timer(TI_SDL,2,1);
			continue;
		}
		sdl.lastfrm=SDL_GetTicks();
		glpaint();
		timer(TI_SDL,3,1);

		if(sdl.cfg.playrecord) sdlsaveframe();
		timer(TI_SDL,4,1);

		SDL_GL_SwapWindow(sdl.wnd);
		timer(TI_SDL,5,1);


		sdlframerate();
		if(!sdl.sync) sdldelay(&paint_last,16);
		else paint_last=SDL_GetTicks();
		timer(TI_SDL,6,1);
	}
	switchdpms(1);
	sdl_quit|=THR_SDL;
	return 0;
}

