
all: release

release:
	scons mode=release

debug:
	scons mode=debug

win: win-release

win-release:
	scons mode=release os=win

win-debug:
	scons mode=debug os=win

clean:
	scons -c

install:
	scons install mode=release destdir="$(DESTDIR)"

uninstall:
	scons -c /

deb:
	debuild
