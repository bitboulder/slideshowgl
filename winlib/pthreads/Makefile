
BINS = bin/pthreadGCE2.dll
LIBS = 
INCS = include/pthread.h include/sched.h

SRCTAR = pthreads-w32-2-8-0-release.tar.bz2
SRCDIR = Pre-built.2
BINDIR = $(SRCDIR)/lib
LIBDIR = $(SRCDIR)/lib
INCDIR = $(SRCDIR)/include

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

$(SRCDIR)/%: $(SRCTAR)
	tar -xjf $< $@

