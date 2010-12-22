BINS = bin/glew32.dll
LIBS = lib/libglew32.dll.a
INCS = include/GL/glew.h

SRCTAR = glew-1.5.7.tgz
SRCDIR = glew-1.5.7/
BINDIR = $(SRCDIR)/lib
LIBDIR = $(SRCDIR)/lib
INCDIR = $(SRCDIR)/include

all: $(BINS) $(LIBS) $(INCS)
	rm -fr $(SRCDIR) libGL.a libGLU.a

clean:
	rm -rf bin lib include $(SRCDIR) libGL.a libGLU.a

bin/%: $(BINDIR)/%
	mkdir -p $(@D)
	cp $< $@

lib/%: $(LIBDIR)/%
	mkdir -p $(@D)
	cp $< $@

include/%: $(INCDIR)/%
	mkdir -p $(@D)
	cp $< $@

$(LIBDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || { tar -xzf $(SRCTAR); patch -d $(SRCDIR) -p1 <glew-1.5.7.patch; }
	make -C $(SRCDIR)

$(BINDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || { tar -xzf $(SRCTAR); patch -d $(SRCDIR) -p1 <glew-1.5.7.patch; }
	make -C $(SRCDIR)

$(SRCDIR)/%: $(SRCTAR)
	[ -d $(SRCDIR) ] || { tar -xzf $(SRCTAR); patch -d $(SRCDIR) -p1 <glew-1.5.7.patch; }

