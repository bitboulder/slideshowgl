
BINS = bin/libintl3.dll
LIBS = lib/libintl.dll.a
INCS = include/libintl.h

SRCBINZIP = gettext-0.14.4-bin.zip
SRCINCZIP = gettext-0.14.4-lib.zip

all: $(BINS) $(LIBS) $(INCS)

clean:
	rm -rf bin lib include

bin/%: $(SRCBINZIP)
	unzip -j -d bin $< $@
	touch $@

lib/%: $(SRCINCZIP)
	unzip -j -d lib $< $@
	touch $@

include/%: $(SRCINCZIP)
	unzip -j -d include $< $@
	touch $@

