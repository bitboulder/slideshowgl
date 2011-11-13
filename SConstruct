env = Environment(APPNAME = 'slideshowgl', VERSION = '2.0.0')

Help("\n")
Help("Type: 'scons mode=release' to build the production program.\n")
Help("Type: 'scons mode=debug' to build the debugging program.\n")
Help("Type: 'scons mode=release os=win' to build the production program for win32.\n")
Help("Type: 'scons mode=debug os=win' to build the debugging program for win32.\n")
Help("Type: 'scons prefix=$PREFIX install' to install with specific prefix.\n")

mode = ARGUMENTS.get('mode')
os   = ARGUMENTS.get('os')

prefix = '/usr'
if ARGUMENTS.get('prefix'):
	prefix = ARGUMENTS.get('prefix')

destdir = ''
if ARGUMENTS.get('destdir'):
	destdir = ARGUMENTS.get('destdir')

def CheckPKGConfig(context, version):
	context.Message( 'Checking for pkg-config... ' )
	ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
	context.Result( ret )
	return ret

def CheckPKG(context, name, demand, define):
	context.Message( 'Checking for %s... ' % name )
	ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
	context.Result( ret )
	if define:
		env['HAVE_'+define] = ret
	if ret:
		env.ParseConfig('pkg-config --cflags --libs \'%s\'' % name.split()[0])
	if demand and not ret:
		print 'Package \'%s\' must be installed!' % name
		Exit(1)
	return ret

def CheckFuncMy(context, name, demand, define):
	ret = conf.CheckFunc(name)
	context.Result('')
	if define:
		env['HAVE_'+define] = ret
	if not ret and demand:
		print 'Function \'%s\' must be available!' % name
		Exit(1)
	return ret

def CheckLibMy(context, name, demand, define):
	ret = conf.CheckLib(name)
	context.Result('')
	if define:
		env['HAVE_'+define] = ret
	if not ret and demand:
		print 'Library \'%s\' must be installed!' % name
		Exit(1)
	return ret


build = 'build'
if os == 'win':
	build = 'build-win'

env.VariantDir(build, '.')

if not env.GetOption('clean'):

	conf = Configure(env, custom_tests = { 'CheckPKGConfig' : CheckPKGConfig, 'CheckPKG' : CheckPKG, 'CheckFuncMy': CheckFuncMy, 'CheckLibMy' : CheckLibMy })

	env['HAVE_EXIF'] = 0
	env['HAVE_EXIV2'] = 0
	env['HAVE_FTGL'] = 0
	env['HAVE_X11'] = 0
	env['HAVE_XEXT'] = 0
	env['HAVE_XINERAMA'] = 0
	env['HAVE_ICONV'] = 0
	env['HAVE_JPEG'] = 0
	env['HAVE_REALPATH'] = 0
	env['HAVE_STRSEP'] = 0
	env['HAVE_GETTEXT'] = 0
	env['HAVE_STAT'] = 0
	env['HAVE_MKTIME'] = 0
	env['HAVE_OPENDIR'] = 0
	env['HAVE_CURL'] = 0

	if os == 'win':
		env.Replace(PROGSUFFIX = '.exe')
		env.Replace(CC        = ['i586-mingw32msvc-gcc'])
		conf.CheckLibMy('mingw32',1,0)

		env.Append(CPPPATH    = ['#/winlib/sdl/include/SDL/'])
		env.Append(LIBPATH    = ['#/winlib/sdl/lib/'])
		conf.CheckLibMy('SDLmain',1,0)
		conf.CheckLibMy('SDL',1,0)

		env.Append(CPPPATH    = ['#/winlib/sdl_image/include-libjpeg/'])
		env.Append(CPPPATH    = ['#/winlib/sdl_image/include/'])
		env.Append(LIBPATH    = ['#/winlib/sdl_image/bin'])
		conf.CheckLibMy('SDL_image',1,0)

		env.Append(CPPPATH    = ['#/winlib/exif/include'])
		env.Append(CPPPATH    = ['#/winlib/exif/include/libexif'])
		env.Append(LIBPATH    = ['#/winlib/exif/lib'])
		conf.CheckLibMy('exif',0,'EXIF')

		env.Append(CPPPATH    = ['#/winlib/ftgl/include'])
		env.Append(CPPPATH    = ['#/winlib/ftgl/include/FTGL'])
		env.Append(LIBPATH    = ['#/winlib/ftgl/lib'])
		conf.CheckLibMy('ftgl',0,'FTGL')

		env.Append(CPPPATH    = ['#/winlib/freetype/include'])
		env.Append(LIBPATH    = ['#/winlib/freetype/lib'])
		conf.CheckLibMy('freetype',0,0)

		env.Append(CPPPATH    = ['#/winlib/gettext/include'])
		env.Append(LIBPATH    = ['#/winlib/gettext/lib'])
		conf.CheckLibMy('intl',0,0)

		env.Append(CPPPATH    = ['#/winlib/iconv/include'])
		env.Append(LIBPATH    = ['#/winlib/iconv/lib'])
		conf.CheckLibMy('iconv',0,'ICONV')

		conf.CheckLibMy('opengl32',1,0)
		conf.CheckLibMy('glu32',1,0)

		env.Append(CPPPATH    = ['#/winlib/glew/include'])
		env.Append(LIBPATH    = ['#/winlib/glew/lib'])
		conf.CheckLibMy('glew32',1,0)

		env.Append(CPPPATH    = ['#/winlib/curl/include'])
		env.Append(LIBPATH    = ['#/winlib/curl/lib'])
		conf.CheckLibMy('curldll',0,'CURL')

		conf.CheckLibMy('stdc++',1,0)
	else:
		conf.CheckPKGConfig('0.2')
		conf.CheckPKG('gl >= 7.0',1,0)
		conf.CheckPKG('glu >= 7.0',1,0)
		conf.CheckPKG('glew >= 1.5',1,0)
		conf.CheckPKG('sdl >= 1.2',1,0)
		conf.CheckPKG('SDL_image >= 1.2',1,0)
		conf.CheckPKG('libexif >= 0.6',0,'EXIF')
		conf.CheckPKG('exiv2 >= 0.2',0,'EXIV2')
		conf.CheckPKG('ftgl >= 2.1',0,'FTGL')
		conf.CheckPKG('x11 >= 1.3',0,'X11')
		conf.CheckPKG('xext >= 1.1',0,'XEXT')
		conf.CheckPKG('xinerama >= 1.0',0,'XINERAMA')
		conf.CheckPKG('libcurl >= 0.2',0,'CURL')
		conf.CheckFuncMy('iconv',0,'ICONV')

	conf.CheckLibMy('jpeg',0,'JPEG')
	conf.CheckFuncMy('realpath',0,'REALPATH')
	conf.CheckFuncMy('strsep',0,'STRSEP')
	conf.CheckLibMy('m',1,0)
	if mode != 'debug':
		conf.CheckFuncMy('gettext',0,'GETTEXT')
	conf.CheckFuncMy('stat',0,'STAT')
	conf.CheckFuncMy('mktime',0,'MKTIME')
	conf.CheckFuncMy('opendir',0,'OPENDIR')

	env = conf.Finish()

env.Append(CCFLAGS = ['-std=c99','-pedantic'])
env.Append(CCFLAGS = ['-Wall'])
env.Append(CCFLAGS = ['-Wextra','-Wno-missing-field-initializers'])
env.Append(CCFLAGS = ['-Wwrite-strings','-Wconversion','-Wunused-macros',
		'-Wdeclaration-after-statement','-Wshadow','-Wmissing-noreturn',
		'-Wmissing-format-attribute'])
#env.Append(CCFLAGS = ['-Wfloat-equal','-Wpadded'])

if mode == 'debug':
	env.Append(CCFLAGS = ['-g'])
else:
	env.Append(CCFLAGS = ['-O2'])
	env.Append(CCFLAGS = ['-g'])

Export('env')
Export('prefix')
Export('destdir')

SConscript(build + '/src/SConscript')
SConscript(build + '/data/SConscript')
SConscript(build + '/doc/SConscript')
SConscript(build + '/po/SConscript')

Import('install_bin')
Import('install_data')
Import('install_doc')
Import('install_po')

env.Alias('install', [install_bin, install_data, install_po, install_doc])
