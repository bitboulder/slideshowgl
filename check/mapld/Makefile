CFLAGS  = -DMAPLDTEST
CFLAGS += -I.. -I../../build/src
CFLAGS += `pkg-config --libs --cflags sdl`
CFLAGS += `pkg-config --libs --cflags libcurl`

mapldtest: mapldtest.c ../../src/mapld.c Makefile
	gcc -g -o mapldtest mapldtest.c ../../src/mapld.c $(CFLAGS)
