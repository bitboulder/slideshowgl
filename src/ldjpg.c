#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <SDL.h>
#include <setjmp.h>
#if HAVE_JPEG
	#include <jpeglib.h>
#endif
#include "ldjpg.h"
#include "main.h"

struct ldjpg_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */

  jmp_buf setjmp_buffer;	/* for return to caller */
};

METHODDEF(void)
ldjpg_error (j_common_ptr cinfo)
{
	struct ldjpg_error_mgr *err = (struct ldjpg_error_mgr *) cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(err->setjmp_buffer, 1);
}


SDL_Surface *JPG_LoadSwap(char *fn){
#if HAVE_JPEG
	FILE *fd;
	struct jpeg_decompress_struct cinfo;
	struct ldjpg_error_mgr jerr;
	JSAMPROW rowptr[1];
	SDL_Surface *img, *ret=NULL;
	if(!(fd=fopen(fn,"rb"))) goto end1;
	cinfo.err=jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit=ldjpg_error;
	jpeg_create_decompress(&cinfo);
	if(setjmp(jerr.setjmp_buffer)) goto end2;
	jpeg_stdio_src(&cinfo,fd);
	if(jpeg_read_header(&cinfo,TRUE)!=JPEG_HEADER_OK) goto end2;
	if(cinfo.num_components==4){
		cinfo.out_color_space=JCS_CMYK;
		cinfo.quantize_colors=FALSE;
		jpeg_calc_output_dimensions(&cinfo);
		img=SDL_AllocSurface(SDL_SWSURFACE,
				cinfo.output_height,cinfo.output_width,32,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		                   0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000
#else
		                   0x0000FF00, 0x00FF0000, 0xFF000000, 0x000000FF
#endif
				);
	}else{
		cinfo.out_color_space=JCS_RGB;
		cinfo.quantize_colors=FALSE;
#ifdef FAST_JPEG
		cinfo.scale_num=1;
		cinfo.scale_denom=1;
		cinfo.dct_method=JDCT_FASTEST;
		cinfo.do_fancy_upsampling=FALSE;
#endif
		jpeg_calc_output_dimensions(&cinfo);
		img=SDL_AllocSurface(SDL_SWSURFACE,
				cinfo.output_height,cinfo.output_width,24,
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
		                   0x0000FF, 0x00FF00, 0xFF0000,
#else
		                   0xFF0000, 0x00FF00, 0x0000FF,
#endif
		                   0);
	}
	if(!img) goto end2;
	if(jpeg_start_decompress(&cinfo)!=TRUE) goto end2;

	while(cinfo.output_scanline<cinfo.output_height){
		rowptr[0]=(JSAMPROW)(Uint8*)img->pixels +
			cinfo.output_scanline*cinfo.output_width*cinfo.output_components;
		jpeg_read_scanlines(&cinfo,rowptr,1);
	}

	jpeg_finish_decompress(&cinfo);
	ret=img;
end2:
	jpeg_destroy_decompress(&cinfo);
	fclose(fd);
end1:
	if(!ret) error(ERR_CONT,"jpeg decompression failure");
	return ret;
#else
	return NULL;
#endif
}
