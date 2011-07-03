
SCONS = scons $(S)

ifneq (,$(findstring s,$(MAKEFLAGS)))
  S = -s
else
  S = 
endif

.PHONY: all release debug win win-release win-debug clean install uninstall deb gettext

all: release

release:
	$(SCONS) mode=release

debug:
	$(SCONS) mode=debug

win: win-release

win-release:
	$(SCONS) mode=release os=win

win-debug:
	$(SCONS) mode=debug os=win

clean:
	$(SCONS) -c

install:
	$(SCONS) install mode=release destdir="$(DESTDIR)"

uninstall:
	$(SCONS) -c /

deb:
	debuild

gettext:
	xgettext -o po/template.pot -k_ -k__ -kerror:2 -kdebug:2 src/*.c src/dpl_help.h
	[ ! -f po/de.po ] && msginit --locale=de -o po/de.po -i po/template.pot || msgmerge --backup=none -U po/de.po po/template.pot
	rm po/template.pot
