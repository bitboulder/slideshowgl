BINS = 
LIBS = lib/libftgl.a
INCS =  include/FTGL/ftgl.h \
		include/FTGL/FTPoint.h \
		include/FTGL/FTBBox.h \
		include/FTGL/FTBuffer.h \
		include/FTGL/FTGlyph.h \
		include/FTGL/FTBitmapGlyph.h \
		include/FTGL/FTBufferGlyph.h \
		include/FTGL/FTExtrdGlyph.h \
		include/FTGL/FTOutlineGlyph.h \
		include/FTGL/FTPixmapGlyph.h \
		include/FTGL/FTPolyGlyph.h \
		include/FTGL/FTTextureGlyph.h \
		include/FTGL/FTFont.h \
		include/FTGL/FTGLBitmapFont.h \
		include/FTGL/FTBufferFont.h \
		include/FTGL/FTGLExtrdFont.h \
		include/FTGL/FTGLOutlineFont.h \
		include/FTGL/FTGLPixmapFont.h \
		include/FTGL/FTGLPolygonFont.h \
		include/FTGL/FTGLTextureFont.h \
		include/FTGL/FTLayout.h \
		include/FTGL/FTSimpleLayout.h
	

SRCTAR = ftgl-2.1.3-rc5.tar.bz2
SRCDIR = ftgl-2.1.3~rc5
BINDIR = $(SRCDIR)/src/.libs
LIBDIR = $(SRCDIR)/src/.libs
INCDIR = $(SRCDIR)/src

all: $(BINS) $(LIBS) $(INCS)
	rm -fr $(SRCDIR) libGL.a libGLU.a

clean:
	rm -rf bin lib include $(SRCDIR) libGL.a libGLU.a

bin/%: $(BINDIR)/%
	mkdir -p `dirname $@`
	cp $< $@

lib/%: $(LIBDIR)/%
	mkdir -p `dirname $@`
	cp $< $@

include/%: $(INCDIR)/%
	mkdir -p `dirname $@`
	cp $< $@

$(BINDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || { tar -xjf $(SRCTAR); patch -d $(SRCDIR) -p1 <ftgl-2.1.3~rc5.patch; }
	ln -sf /usr/i586-mingw32msvc/lib/libopengl32.a libGL.a
	ln -sf /usr/i586-mingw32msvc/lib/libglu32.a libGLU.a
	cd $(SRCDIR); CPPFLAGS=-I`realpath ../../freetype/include` LDFLAGS=-L`realpath ..` ./configure --host=i586-mingw32msvc
	make -C $(SRCDIR)
	rm libGL.a libGLU.a

$(SRCDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || { tar -xjf $(SRCTAR); patch -d $(SRCDIR) -p1 <ftgl-2.1.3~rc5.patch; }

