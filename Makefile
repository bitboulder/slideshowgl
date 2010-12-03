
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

gettext:
	xgettext -o po/template.pot -k_ -k__ -kerror:2 -kdebug:2 src/*.c
	[ ! -f po/de.po ] && msginit --locale=de -o po/de.po -i po/template.pot || msgmerge --backup=none -U po/de.po po/template.pot
	rm po/template.pot
