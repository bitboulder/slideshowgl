
BINS = bin/SDL_image.dll bin/jpeg.dll bin/libpng12-0.dll bin/libtiff-3.dll bin/zlib1.dll
LIBS = 
INCS = include/SDL_image.h
INCS+= include-libjpeg/jconfig.h include-libjpeg/jmorecfg.h include-libjpeg/jpeglib.h

SRCBINZIP = SDL_image-1.2.10-win32.zip
SRCINCZIP = SDL_image-1.2.10.zip
SRCINCDIR = SDL_image-1.2.10

all: $(BINS) $(LIBS) $(INCS)

clean:
	rm -rf bin lib include include-libjpeg

bin/%: $(SRCBINZIP)
	unzip -j -d bin $< `basename $@`
	touch $@

include/%: $(SRCINCZIP)
	unzip -j -d include $< $(SRCINCDIR)/`basename $@`
	touch $@

include-libjpeg/%: /usr/include/%
	mkdir -p `dirname $@`
	cp $< $@
