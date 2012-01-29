
BINS = bin/libcurl.dll bin/libidn-11.dll bin/libeay32.dll bin/libssl32.dll
LIBS = lib/libcurldll.a
INCS = include/curl/curl.h \
	   include/curl/curlver.h \
	   include/curl/curlbuild.h \
	   include/curl/curlrules.h \
	   include/curl/easy.h \
	   include/curl/multi.h \
	   include/curl/typecheck-gcc.h \


SRCZIP = curl-7.21.7-devel-mingw32.zip

all: $(BINS) $(LIBS) $(INCS)

clean:
	rm -rf bin lib include

bin/%: $(SRCZIP)
	unzip -j -d bin $< curl-7.21.7-devel-mingw32/$@
	touch $@

lib/%: $(SRCZIP)
	unzip -j -d lib $< curl-7.21.7-devel-mingw32/$@
	touch $@

include/%: $(SRCZIP)
	mkdir -p include
	unzip -j -d include/curl $< curl-7.21.7-devel-mingw32/$@
	touch $@

