#include "config.h"
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

char sdl_quit = 0;

SDL_Surface *screen;

struct sdlcfg {
	Uint32 hidecursor;
	Uint32 doubleclicktime;
	int fsaa;
	const char *playrecord;
};

struct sdlmove {
	Uint16 base_x, base_y;
	int pos_x, pos_y;
	char jump;
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
	struct sdlclickdelay {
		Uint32 time;
		Uint8 btn;
		Uint16 x,y;
		int clickimg;
	} clickdelay;
} sdl = {
	.fullscreen = 0,
	.scr_w      = 0,
	.scr_h      = 0,
	.doresize   = 0,
	.sync       = 0,
	.hidecursor = 0,
	.move.base_x= 0xffff,
	.clickdelay.time = 0,
};

/* thread: gl */
float sdlrat(){ return (float)sdl.scr_w/(float)sdl.scr_h; }

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
void sdlfullscreen(char dst){
	if(dst==sdl.fullscreen) return;
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
	Display *display=XOpenDisplay(NULL);
	XineramaScreenInfo *info;
	int ninfo,i;
	if(display && XineramaIsActive(display) && (info=XineramaQueryScreens(display,&ninfo))){
		*w=*h=0;
		for(i=0;i<ninfo;i++){
			//info[i].screen_number
			if(info[i].x_org+info[i].width> *w) *w=info[i].x_org+info[i].width;
			if(info[i].y_org+info[i].height>*h) *h=info[i].y_org+info[i].height;
		}
		free(info);
		if(*w && *h) return 1;
	}
	XCloseDisplay(display);
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
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,sdl.cfg.fsaa?1:0);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES,sdl.cfg.fsaa);
	}
	if(sdl.fullscreen && sdlgetfullscreenmode(flags|SDL_FULLSCREEN,&w,&h)){
		debug(DBG_STA,"sdl set video mode fullscreen %ix%i",w,h);
		flags|=SDL_FULLSCREEN;
	}else{
		if(!w) w=sdl.scr_w;
		if(!h) h=sdl.scr_h;
		debug(DBG_STA,"sdl set video mode %ix%i",sdl.scr_w,sdl.scr_h);
		flags|=SDL_RESIZABLE;
	}
	if(!(screen=SDL_SetVideoMode(w,h,16,flags))){
		if(!done && sdl.cfg.fsaa){
			error(ERR_CONT,"disable anti-aliasing");
			sdl.cfg.fsaa=0;
			sdlresize(w,h);
			return;
		}else error(ERR_QUIT,"video mode init failed");
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
	SDL_WM_SetIcon(icon,NULL);
	SDL_FreeSurface(icon);
}

void sdlinit(){
	sdl.sync=cfggetbool("sdl.sync");
	sdl.fullscreen=cfggetbool("sdl.fullscreen");
	sdl.cfg.hidecursor=cfggetuint("sdl.hidecursor");
	sdl.scrnof_w=cfggetint("sdl.width");
	sdl.scrnof_h=cfggetint("sdl.height");
	sdl.cfg.doubleclicktime=cfggetuint("sdl.doubleclicktime");
	sdl.cfg.fsaa=cfggetint("sdl.fsaa");
	sdl.cfg.playrecord=cfggetstr("sdpl.playrecord");
	if(sdl.cfg.playrecord && sdl.cfg.playrecord[0]){
		sdl.scrnof_w=cfggetint("sdl.playrecord_w");
		sdl.scrnof_h=cfggetint("sdl.playrecord_h");
		sdl.fullscreen=0;
	}else sdl.cfg.playrecord=NULL;
	if(SDL_Init(SDL_INIT_TIMER|SDL_INIT_VIDEO)<0) error(ERR_QUIT,"sdl init failed");
	SDL_EnableUNICODE(1);
	if(cfggetint("cfg.version")){
		const SDL_version* v = SDL_Linked_Version();
		char buf[32];
		mprintf("SDL-Version: %i.%i.%i (video: %s)\n",v->major,v->minor,v->patch,
				SDL_VideoDriverName(buf,32)?buf:_("(unknown)"));
	}
	sdlicon();
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
		case SDLK_KP_ENTER: dplevputk(' ');       break;
		case SDLK_HOME:     dplevputi(DE_SEL,0); break;
		case SDLK_END:      dplevputi(DE_SEL,IMGI_END-1); break;
		default:            if(key.unicode) dplevputk(key.unicode); break;
	}
}

char sdljump(Uint16 x,Uint16 y,char end){
	int xd=x-sdl.move.base_x, yd=y-sdl.move.base_y;
	int zoom=dplgetzoom();
	int w=100,wthr;
	enum dplev ev=0;
	int actil=dplgetactil();
	if(!sdl.move.jump && abs(x-sdl.move.base_x)<10 && abs(y-sdl.move.base_y)<10) return 0;
	sdl.move.jump=1;
	if(zoom<=0 && actil<1){
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
			dplevputp(DE_JUMP,
				-(float)xd/(float)sdl.scr_w,
				-(float)yd/(float)sdl.scr_h);
		sdl.move.base_x=x;
		sdl.move.base_y=y;
		if(end) dplevput(DE_JUMPEND);
	}
	return 1;
}

void sdlclick(Uint8 btn,Uint16 x,Uint16 y,int clickimg){
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
	}else switch(btn){
		case SDL_BUTTON_LEFT:
			if(zoom>0)           dplevputp(DE_MOVE,sx,sy);
			else if(doubleclick) dplevputi(DE_DIR|DE_ZOOMIN,clickimg);
			else if(zoom==0)     dplevputs(DE_RIGHT,DES_MOUSE);
			else                 dplevputi(DE_SEL,clickimg);
		break;
		case SDL_BUTTON_MIDDLE: dplevputi(DE_MARK|DE_STOP|DE_PLAY,clickimg); break;
		case SDL_BUTTON_RIGHT: if(zoom==0) dplevputs(DE_LEFT,DES_MOUSE); break;
	}
}

void sdlmotion(Uint16 x,Uint16 y){
	float fx = (float)x/(float)sdl.scr_w - .5f;
	float fy = (float)y/(float)sdl.scr_h - .5f;
	SDL_ShowCursor(SDL_ENABLE);
	sdl.hidecursor=SDL_GetTicks()+sdl.cfg.hidecursor;
	//printixy((float)x/(float)sdl.scr_w-.5f,(float)y/(float)sdl.scr_h-.5f);
	if(sdl.move.base_x!=0xffff) sdljump(x,y,0);
	else if(dplgetactil()<0) dplevputs(DE_STAT,DES_MOUSE);
	else dplevputx(DE_STAT,0,fx,fy,glselect(x,y),DES_MOUSE);
}

void sdlbutton(char down,Uint8 button,Uint16 x,Uint16 y){
	float fx = (float)x/(float)sdl.scr_w - .5f;
	float fy = (float)y/(float)sdl.scr_h - .5f;
	int clickimg = glselect(x,y);
	if(down) switch(button){
		case SDL_BUTTON_LEFT:
			sdl.move.jump=0;
			sdl.move.pos_x=0;
			sdl.move.pos_y=0;
			sdl.move.base_x=x;
			sdl.move.base_y=y;
		break;
		case SDL_BUTTON_MIDDLE:
		case SDL_BUTTON_RIGHT:     sdlclick(button,x,y,clickimg);         break;
		case SDL_BUTTON_WHEELUP:   dplevputpi(DE_ZOOMIN, fx,fy,clickimg); break;
		case SDL_BUTTON_WHEELDOWN: dplevputpi(DE_ZOOMOUT,fx,fy,clickimg); break;
	}else if(button==SDL_BUTTON_LEFT){
		if(!sdljump(x,y,1)) sdlclick(button,x,y,clickimg);
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
		if(sdl.doresize) sdlresize(0,0);
		sdlhidecursor();
		timer(TI_SDL,0,1);
		
		if(!effineff()) ldtexload();
		while(SDL_GetTicks()-paint_last < (effineff()?6:20)) if(!ldtexload()) break;
		timer(TI_SDL,1,1);

		glpaint();
		timer(TI_SDL,2,1);

		if(sdl.cfg.playrecord) sdlsaveframe();
		timer(TI_SDL,3,1);

		SDL_GL_SwapBuffers();
		timer(TI_SDL,4,1);


		sdlframerate();
		if(!sdl.sync) sdldelay(&paint_last,16);
		else paint_last=SDL_GetTicks();
		timer(TI_SDL,5,1);
	}
	switchdpms(1);
	sdl_quit|=THR_SDL;
	return 0;
}

