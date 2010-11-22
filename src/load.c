#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include <pthread.h>
#if HAVE_REALPATH
	#include <sys/param.h>
	#include <unistd.h>
#endif
#include "load.h"
#include "sdl.h"
#include "main.h"
#include "help.h"
#include "img.h"
#include "dpl.h"
#include "exif.h"
#include "cfg.h"
#include "act.h"

/***************************** load *******************************************/

struct load {
	int  texsize[TEX_NUM];
	char vartex;
	pthread_mutex_t mutex;
} load = {
	.texsize = { 256, 512, 2048, 8192, },
	/* ms:       1.2  1.2    20 */
	.vartex = 0,
};

/* thread: gl */
void ldmaxtexsize(){
	GLint maxtex;
	int i;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxtex); 
	for(i=0;i<TEX_NUM;i++) if(load.texsize[i]>maxtex){
		if(i && load.texsize[i-1]>=maxtex) load.texsize[i]=0;
		else load.texsize[i]=maxtex;
	}
}

/* thread: gl */
#define VT_W	67
#define VT_H	75
void ldcheckvartex(){
	GLuint tex=0;
	char dat[3*VT_W*VT_H];
	glGenTextures(1,&tex);
	glBindTexture(GL_TEXTURE_2D,tex);
	glTexImage2D(GL_TEXTURE_2D,0,3,VT_W,VT_H,0,GL_RGB,GL_UNSIGNED_BYTE,dat);
	glDeleteTextures(1,&tex);
	load.vartex=!glGetError();
}

/***************************** imgld ******************************************/

struct itex {
	GLuint tex;
	char loading;
	char thumb;
	char refresh;
};

struct imgld {
	char fn[1024];
	char tfn[1024];
	char loadfail;
	int w,h;
	struct itex texs[TEX_NUM];
	struct img *img;
	struct ipano pano;
};

/* thread: img */
struct imgld *imgldinit(struct img *img){
	struct imgld *il=calloc(1,sizeof(struct imgld));
	il->img=img;
	return il;
}

/* thread: img */
void imgldfree(struct imgld *il){
	int i;
	for(i=0;i<TEX_NUM;i++) if(il->texs[i].tex) glDeleteTextures(1,&il->texs[i].tex);
	free(il);
}

/* thread: img */
void imgldsetimg(struct imgld *il,struct img *img){ il->img=img; }

/* thread: gl */
GLuint imgldtex(struct imgld *il,enum imgtex it){
	int i;
	for(i=it;i>=0;i--) if(il->texs[i].tex) return il->texs[i].tex;
	for(i=it;i<TEX_NUM;i++) if(il->texs[i].tex) return il->texs[i].tex;
	return 0;
}

/* thread: dpl, load, gl */
float imgldrat(struct imgld *il){
	return (!il->h || !il->w) ? 0. : (float)il->h/(float)il->w;
}

/* thread: dpl, load */
char *imgldfn(struct imgld *il){ return il->fn; }

void imgldrefresh(struct imgld *il){
	int i;
	for(i=0;i<TEX_NUM;i++) il->texs[i].refresh=1;
}

/* thread: gl */
struct ipano *imgldpano(struct imgld *il){ return il->pano.enable ? &il->pano : NULL; }

/***************************** sdlimg *****************************************/

struct sdlimg {
	SDL_Surface *sf;
	int ref;
};

void sdlimg_unref(struct sdlimg *sdlimg){
	if(!sdlimg) return;
	if(--sdlimg->ref) return;
	SDL_FreeSurface(sdlimg->sf);
	free(sdlimg);
}

void sdlimg_ref(struct sdlimg *sdlimg){
	if(!sdlimg) return;
	sdlimg->ref++;
}

struct sdlimg* sdlimg_gen(SDL_Surface *sf){
	struct sdlimg *sdlimg;
	if(!sf) return NULL;
	sdlimg=malloc(sizeof(struct sdlimg));
	sdlimg->sf=sf;
	sdlimg->ref=1;
	return sdlimg;
}

/***************************** texload ****************************************/

struct texload {
	struct itex *itex;
	struct sdlimg *sdlimg;
};
#define TEXLOADNUM	16
struct texloadbuf {
	struct texload tl[TEXLOADNUM];
	int wi,ri;
} tlb = {
	.wi=0,
	.ri=0,
};
void ldtexload_put(struct itex *itex,struct sdlimg *sdlimg){
	int nwi=(tlb.wi+1)%TEXLOADNUM;
	pthread_mutex_lock(&load.mutex);
	while(nwi==tlb.ri){
		SDL_Delay(10);
		if(sdl.quit) return;
	}
	itex->loading=1;
	tlb.tl[tlb.wi].itex=itex;
	tlb.tl[tlb.wi].sdlimg=sdlimg;
	tlb.wi=nwi;
	pthread_mutex_unlock(&load.mutex);
}

/* thread: sdl */
char ldtexload(){
	GLenum glerr;
	struct texload *tl;
	if(tlb.wi==tlb.ri) return 0;
	tl=tlb.tl+tlb.ri;
	if(tl->sdlimg){
		if(dplineff() && (tl->sdlimg->sf->w>=1024 || tl->sdlimg->sf->h>=1024)) return 0;
		//timer(-2);
		if(!tl->itex->tex) glGenTextures(1,&tl->itex->tex);
		glBindTexture(GL_TEXTURE_2D, tl->itex->tex);
		glTexImage2D(GL_TEXTURE_2D, 0, 3, tl->sdlimg->sf->w, tl->sdlimg->sf->h, 0, GL_RGB, GL_UNSIGNED_BYTE, tl->sdlimg->sf->pixels);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//timer(MAX(tl->sdlimg->sf->w,tl->sdlimg->sf->h)/256);
		sdlimg_unref(tl->sdlimg);
		//timer(0);
	}else{
		if(tl->itex->tex) glDeleteTextures(1,&tl->itex->tex);
		tl->itex->tex=0;
	}
	if((glerr=glGetError())){
		error(ERR_CONT,"glTexImage2D %s failed (gl-err: %d)",tl->sdlimg?"load":"free",glerr);
		if(tl->sdlimg) tl->itex->tex=0;
	}
	tl->itex->loading=0;
	tlb.ri=(tlb.ri+1)%TEXLOADNUM;
	return 1;
}

/***************************** load + free img ********************************/

int sizesel(int sdemand,int simg){
	int size=64;
	if(sdemand<simg) return sdemand;
	if(load.vartex) return simg;
	while(size<simg*0.95) size<<=1;
	return size;
}

char ldfload(struct imgld *il,enum imgtex it){
	struct sdlimg *sdlimg;
	int i;
	char ld=0;
	char *fn = il->fn;
	char thumb=0;
	struct icol *icol;
	if(il->loadfail) return ld;
	while(it>=0 && !load.texsize[it]) it--;
	if(it<0) return ld;
	if(il->texs[it].loading) return ld;
	if(il->texs[it].tex && !il->texs[it].refresh) return ld;
	if(it==TEX_FULL && il->pano.enable) return ld;
	debug(DBG_STA,"ld loading img tex %s %s",imgtex_str[it],il->fn);
	imgexifload(il->img->exif,il->fn);
	if(it<TEX_BIG && il->tfn[0]){
		fn=il->tfn;
		thumb=1;
	}
	debug(DBG_DBG,"ld Loading img \"%s\"",fn);
	sdlimg=sdlimg_gen(IMG_Load(fn));
	if(!sdlimg){ error(ERR_CONT,"Loading img failed \"%s\": %s",fn,IMG_GetError()); goto end; }
	if(sdlimg->sf->format->BytesPerPixel!=3){ error(ERR_CONT,"Wrong pixelformat \"%s\"",fn); goto end; }

	icol=imgposcol(il->img->pos);
	if(icol->g) SDL_ColMod(sdlimg->sf,icol->g,0.f,0.f);

	il->w=sdlimg->sf->w;
	il->h=sdlimg->sf->h;
	debug(DBG_DBG,"ld img size %ix%i \"%s\"",il->w,il->h,il->fn);
	effrefresh(EFFREF_FIT);

	for(i=it;i>=0;i--){
		struct sdlimg *sdlimgscale;
		int size=64;
		if(il->texs[i].tex && !il->texs[i].refresh && (thumb || i!=TEX_SMALL || !il->texs[i].thumb)) continue;
		if(il->texs[i].loading) continue;
		if(i<TEX_NUM && !(size=load.texsize[i])) continue;
		if((sdlimgscale = sdlimg_gen(SDL_ScaleSurface(sdlimg->sf,sizesel(size,il->w),sizesel(size,il->h))))){
			sdlimg_unref(sdlimg);
			sdlimg=sdlimgscale;
		}
		sdlimg_ref(sdlimg);
		il->texs[i].thumb=thumb;
		il->texs[i].refresh=0;
		debug(DBG_DBG,"ld Loading to tex %s (%ix%i)",imgtex_str[i],sdlimg->sf->w,sdlimg->sf->h);
		ldtexload_put(il->texs+i,sdlimg);
		ld=1;
	}
	sdlimg_unref(sdlimg);
	return ld;
end:
	sdlimg_unref(sdlimg);
	il->loadfail=1;
	return ld;
}

char ldffree(struct imgld *il,enum imgtex thold){
	int it;
	char ret=0;
	for(it=thold+1;it<TEX_NUM;it++) if(il->texs[it].tex){
		if(il->texs[it].loading) continue;
		debug(DBG_STA,"ld freeing img tex %s %s",imgtex_str[it],il->fn);
		ldtexload_put(il->texs+it,NULL);
		ret=1;
	}
	return ret;
}

/***************************** load concept *************************************/

struct loadconcept {
	char *load_str;
	char *hold_str;
	struct loadconept_ld {
		int imgi;
		enum imgtex tex;
	} *load;
	enum imgtex *hold;
	int hold_min;
	int hold_max;
};

struct loadconcept loadconcepts[] = {
	{ "F:0,B:0..2,-1;S:+-5,+-10,3..4,-2..-4", "F:0,B:-2..4;S:..15;T:..24" }, /* zoom >  0 */
	{ "B:0..2,-1;S:+-5,+-10,3..4,-2..-4",     "B:-2..4;S:..15;T:..24"     }, /* zoom =  0 */
	{ "S:..17;B:0,1",                         "B:-1..2;S:..22;T:..38"     }, /* zoom = -1 */
	{ "S:..22;T:+-23..31;B:0",                "B:..1;S:..37;T:..52"       }, /* zoom = -2 */
	{ "T:..38;S:..22;B:0",                    "B:..1;S:..37;T:..59"       }, /* zoom = -3 */
};
#define NUM_CONCEPTS (sizeof(loadconcepts)/sizeof(struct loadconcept))

struct loadconept_ld *ldconceptadd(int imgi,enum imgtex tex){
	static int num=0,size=0;
	static struct loadconept_ld *ret=NULL;

	if(num==size) ret=realloc(ret,sizeof(struct loadconept_ld)*(size+=32));
	ret[num].imgi=imgi;
	ret[num].tex=tex;
	num++;

	if(tex==TEX_NONE){
		struct loadconept_ld *retl=ret;
		num=size=0;
		ret=NULL;
		return retl;
	}else return NULL;
}

struct loadconept_ld *ldconceptstr(char *str){
	char strcopy[1024];
	char *tok;
	enum imgtex tex=TEX_NONE;
	snprintf(strcopy,1024,str);
	str=strcopy;
	while((tok=strsep(&str,",;"))){
		char sign=0;
		int  istr,iend;
		char *tokend;
		if(tok[1]==':'){
			switch(tok[0]){
				case 'F': tex=TEX_FULL; break;
				case 'B': tex=TEX_BIG; break;
				case 'S': tex=TEX_SMALL; break;
				case 'T': tex=TEX_TINY; break;
				default: error(ERR_QUIT,"concept: unknown tex '%c'",tok[0]);
			}
			tok+=2;
		}
		if(tok[0]=='.' && tok[1]=='.'){
			iend=strtol(tok+2,&tokend,0);
			if(tokend==tok+2) error(ERR_QUIT,"concept: no digits after '..': '%s'",tokend);
			if(tokend[0]) error(ERR_QUIT,"concept: str after '..[0-9]+': '%s'",tokend);
			if(iend<0) iend*=-1;
			for(istr=0;istr<=iend;istr++){
				ldconceptadd(istr,tex);
				ldconceptadd(-istr,tex);
			}
			continue;
		}
		if(tok[0]=='+' && tok[1]=='-'){ sign=1; tok+=2; }
		istr=strtol(tok,&tokend,0);
		if(tokend==tok) error(ERR_QUIT,"concept: no digits at begin: '%s'",tokend);
		if(tokend[-1]=='.') tokend--;
		tok=tokend;
		if(tok[0]=='.' && tok[1]=='.'){
			iend=strtol(tok+2,&tokend,0);
			if(tokend==tok+2) error(ERR_QUIT,"concept: no digits after '[0-9]+..': '%s'",tokend);
			if(tokend[0]) error(ERR_QUIT,"concept: str after '[0-9]+..[0-9]+': '%s'",tokend);
		}else{
			if(tok[0]) error(ERR_QUIT,"concept: str after '[0-9]+': '%s' (%s)",tok,strcopy);
			iend=istr;
		}
		while(1){
			ldconceptadd(istr,tex);
			if(sign) ldconceptadd(-istr,tex);
			if(istr==iend) break;
			istr += istr<iend ? 1 : -1;
		}
	}
	return ldconceptadd(0,TEX_NONE);
}

void ldconceptcompile(){
	int z;
	for(z=0;z<NUM_CONCEPTS;z++){
		int i,hmin,hmax;
		struct loadconept_ld *hold;
		loadconcepts[z].load = ldconceptstr(loadconcepts[z].load_str);
		hold = ldconceptstr(loadconcepts[z].hold_str);
		for(hmin=hmax=i=0;hold[i].tex!=TEX_NONE;i++){
			if(hold[i].imgi<hmin) hmin=hold[i].imgi;
			if(hold[i].imgi>hmax) hmax=hold[i].imgi;
		}
		loadconcepts[z].hold_min = hmin;
		loadconcepts[z].hold_max = hmax;
		loadconcepts[z].hold=malloc(sizeof(enum imgtex)*(hmax-hmin+1));
		memset(loadconcepts[z].hold,-1,sizeof(enum imgtex)*(hmax-hmin+1));
		for(i=0;hold[i].tex!=TEX_NONE;i++) if(loadconcepts[z].hold[hold[i].imgi-hmin]<hold[i].tex)
			loadconcepts[z].hold[hold[i].imgi-hmin]=hold[i].tex;
		free(hold);
	}
}

void ldconceptfree(){
	int z;
	for(z=0;z<NUM_CONCEPTS;z++){
		free(loadconcepts[z].load);
		free(loadconcepts[z].hold);
	}
}

/***************************** load check *************************************/

char ldcheck(){
	int i;
	int imgi = dplgetimgi();
	int zoom = dplgetzoom();
	struct loadconcept *ldcp;
	char ret=0;

	if(zoom>0) ldcp=loadconcepts+0;
	else if(zoom==0) ldcp=loadconcepts+1;
	else if(1-zoom<NUM_CONCEPTS) ldcp=loadconcepts+(1-zoom);
	else ldcp=loadconcepts+NUM_CONCEPTS-1;

	for(i=0;i<nimg;i++){
		enum imgtex hold=TEX_NONE;
		int diff=i-imgi;
		if(dplloop()){
			while(diff>nimg/2) diff-=nimg;
			while(diff<-nimg/2) diff+=nimg;
		}
		if(diff>=ldcp->hold_min && diff<=ldcp->hold_max)
			hold=ldcp->hold[diff-ldcp->hold_min];
		if(ldffree(imgs[i]->ld,hold)){ ret=1; break; }
	}

	for(i=0;ldcp->load[i].tex!=TEX_NONE;i++){
		int imgri;
		struct img *img;
		enum imgtex tex;
		imgri=imgi+ldcp->load[i].imgi;
		if(dplloop()) imgri=(imgri+nimg)%nimg;
		img=imgget(imgri);
		tex=ldcp->load[i].tex;
		if(img && ldfload(img->ld,tex)){ ret=1; break; }
	}

	return ret;
}

/***************************** load thread ************************************/

extern Uint32 paint_last;

void *ldthread(void *arg){
	pthread_mutex_init(&load.mutex,NULL);
	ldconceptcompile();
	ldfload(defimg->ld,TEX_BIG);
	while(!sdl.quit){
		if(!ldcheck()) SDL_Delay(100); else if(dplineff()) SDL_Delay(20);
		panocheck();
		sdlthreadcheck();
	}
	ldconceptfree();
	sdl.quit|=THR_LD;
	return NULL;
}

/***************************** load files init ********************************/

void ldpanoinit(struct imgld *il){
	char cmd[1024];
	FILE *fd;
	snprintf(cmd,512,"/mnt/img/self/scriptz/pano.pl %s 2>/dev/null",il->fn);
	if(!(fd=popen(cmd,"r"))) return;
	il->pano.rotinit=4.f;
	fscanf(fd,"%f %f %f %f",&il->pano.gw,&il->pano.gh,&il->pano.gyoff,&il->pano.rotinit);
	pclose(fd);
	il->pano.enable = il->pano.gw && il->pano.gh;
}

void ldfindthumb(struct img *img,char *fn){
	int i;
	FILE *fd;
	for(i=strlen(fn)-1;i>=0;i--) if(fn[i]=='/'){
		fn[i]='\0';
		snprintf(img->ld->tfn,1024,"%s/thumb/%s",fn,fn+i+1);
		fn[i]='/';
		if((fd=fopen(img->ld->tfn,"rb"))){ fclose(fd); break; }
	}
	if(i<0) img->ld->tfn[0]='\0';
}

void ldaddfile(char *fn){
	struct img *img=imgadd();
	if(!strncmp(fn,"file://",7)) fn+=7;
	strncpy(img->ld->fn,fn,1024);
	ldfindthumb(img,fn);
#if HAVE_REALPATH
	if(!img->ld->tfn[0]){
		static char rfn[MAXPATHLEN];
		if((fn=realpath(fn,rfn))) ldfindthumb(img,fn);
	}
#endif
	ldpanoinit(img->ld);
}

void ldaddflst(char *flst){
	FILE *fd=fopen(flst,"r");
	char buf[1024];
	if(!fd){ error(ERR_CONT,"ld read flst failed \"%s\"",flst); return; }
	while(!feof(fd)){
		int len;
		if(!fgets(buf,1024,fd)) continue;
		len=strlen(buf);
		while(buf[len-1]=='\n' || buf[len-1]=='\r') buf[--len]='\0';
		ldaddfile(buf);
	}
	fclose(fd);
}

void ldgetfiles(int argc,char **argv){
	defimg=imginit();
	strncpy(defimg->ld->fn,"data/defimg.png",1024);
	for(;argc;argc--,argv++){
		if(!strcmp(".flst",argv[0]+strlen(argv[0])-5)) ldaddflst(argv[0]);
		else ldaddfile(argv[0]);
	}
	if(cfggetint("ld.random")) imgrandom();
	actadd(ACT_LOADMARKS,NULL);
}
