
BINS = bin/libiconv2.dll
LIBS = lib/libiconv.dll.a
INCS = include/iconv.h

SRCBINZIP = libiconv-1.9.2-1-bin.zip
SRCINCZIP = libiconv-1.9.2-1-lib.zip

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

