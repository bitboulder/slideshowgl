BINS = bin/libfreetype-6.dll
LIBS = lib/libfreetype.dll.a
INCS =  include/ft2build.h \
		include/freetype/config/ftheader.h \
		include/freetype/freetype.h \
		include/freetype/ftglyph.h \
		include/freetype/ftoutln.h \
		include/freetype/config/ftconfig.h \
		include/freetype/fterrors.h \
		include/freetype/fttypes.h \
		include/freetype/config/ftoption.h \
		include/freetype/config/ftstdlib.h \
		include/freetype/ftmoderr.h \
		include/freetype/fterrdef.h \
		include/freetype/ftsystem.h \
		include/freetype/ftimage.h

SRCTAR = freetype-2.4.3.tar.bz2
SRCDIR = freetype-2.4.3
BINDIR = $(SRCDIR)/objs/.libs
LIBDIR = $(SRCDIR)/objs/.libs
INCDIR = $(SRCDIR)/include

all: $(BINS) $(LIBS) $(INCS)
	rm -fr $(SRCDIR)

clean:
	rm -rf bin lib include $(SRCDIR)

bin/%: $(BINDIR)/%
	mkdir -p $(@D)
	cp $< $@

lib/%: $(LIBDIR)/%
	mkdir -p $(@D)
	cp $< $@

include/%: $(INCDIR)/%
	mkdir -p $(@D)
	cp $< $@

$(BINDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || tar -xjf $(SRCTAR)
	cd $(SRCDIR); ./configure --host=i586-mingw32msvc
	make -C $(SRCDIR)

$(SRCDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || tar -xjf $(SRCTAR)

