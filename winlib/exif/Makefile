BINS = bin/libexif-12.dll
LIBS = lib/libexif.dll.a
INCS =  include/libexif/exif-data.h \
		include/libexif/exif-byte-order.h \
		include/libexif/exif-data-type.h \
		include/libexif/exif-ifd.h \
		include/libexif/exif-log.h \
		include/libexif/exif-tag.h \
		include/libexif/exif-content.h \
		include/libexif/exif-mnote-data.h \
		include/libexif/exif-mem.h \
		include/libexif/exif-utils.h \
		include/libexif/exif-format.h \
		include/libexif/exif-entry.h \
		include/libexif/_stdint.h

SRCTAR = libexif-0.6.19.tar.bz2
SRCDIR = libexif-0.6.19
BINDIR = $(SRCDIR)/libexif/.libs
LIBDIR = $(SRCDIR)/libexif/.libs
INCDIR = $(SRCDIR)

all: $(BINS) $(LIBS) $(INCS)
	rm -fr $(SRCDIR)

clean:
	rm -rf bin lib include $(SRCDIR)

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
	[ -d $(SRCDIR) ] || tar -xjf $(SRCTAR)
	cd $(SRCDIR); ./configure --host=i586-mingw32msvc
	make -C $(SRCDIR)

$(SRCDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || tar -xjf $(SRCTAR)

