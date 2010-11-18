env = Environment(APPNAME = 'slideshowgl', VERSION = '0.5.0')

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

def CheckPKGConfig(context, version):
	context.Message( 'Checking for pkg-config... ' )
	ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
	context.Result( ret )
	return ret

def CheckPKG(context, name, demand, define):
	context.Message( 'Checking for %s... ' % name )
	ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
	context.Result( ret )
	if ret:
		env.ParseConfig('pkg-config --cflags --libs \'%s\'' % name.split()[0])
		if define:
			env.Append(CPPDEFINES = ['HAVE_' + define])
	if demand and not ret:
		print 'Package \'%s\' must be installed!' % name
		Exit(1)
	return ret

build = 'build'
if os == 'win':
	build = 'build-win'

env.VariantDir(build, '.')

if not env.GetOption('clean'):

	conf = Configure(env, custom_tests = { 'CheckPKGConfig' : CheckPKGConfig, 'CheckPKG' : CheckPKG })
	conf.CheckLib('m')

	if os == 'win':
		env.Replace(PROGSUFFIX = '.exe')
		env.Replace(CC        = ['i586-mingw32msvc-gcc'])
		env.Append(LIBS       = ['mingw32'])

		env.Append(CPPPATH    = ['#/winlib/sdl/SDL-1.2.14/include/SDL/'])
		env.Append(LIBPATH    = ['#/winlib/sdl/SDL-1.2.14/lib/'])
		env.Append(LIBS       = ['SDLmain'])
		env.Append(LIBS       = ['SDL'])

		env.Append(CPPPATH    = ['#/winlib/sdl_image/SDL_image-1.2.10/'])
		env.Append(LIBPATH    = ['#/winlib/sdl_image/lib/'])
		env.Append(LIBS       = ['SDL_image'])

		env.Append(CPPPATH    = ['#/winlib/pthreads/Pre-built.2/include'])
		env.Append(LIBPATH    = ['#/winlib/pthreads/Pre-built.2/lib'])
		env.Append(LIBS       = ['pthreadGCE2'])

		env.Append(CPPPATH    = ['#/winlib/exif/libexif-0.6.19'])
		env.Append(CPPPATH    = ['#/winlib/exif/libexif-0.6.19/libexif'])
		env.Append(LIBPATH    = ['#/winlib/exif/libexif-0.6.19/libexif/.libs'])
		env.Append(LIBS       = ['exif'])
		env.Append(CPPDEFINES = ['HAVE_EXIF'])

		env.Append(CPPPATH    = ['#/winlib/ftgl/ftgl-2.1.3~rc5/src'])
		env.Append(CPPPATH    = ['#/winlib/ftgl/ftgl-2.1.3~rc5/src/FTGL'])
		env.Append(LIBPATH    = ['#/winlib/ftgl/ftgl-2.1.3~rc5/src/.libs'])
		env.Append(LIBS       = ['ftgl'])
		env.Append(CPPPATH    = ['#/winlib/ftgl/freetype-2.4.3/include'])
		env.Append(LIBPATH    = ['#/winlib/ftgl/freetype-2.4.3/objs/.libs'])
		env.Append(LIBS       = ['freetype'])
		env.Append(CPPDEFINES = ['HAVE_FTGL'])

		env.Append(LIBS       = ['opengl32'])
		env.Append(LIBS       = ['glu32'])
		env.Append(LIBS       = ['stdc++'])
	else:
		conf.CheckCHeader('pthread.h')
		conf.CheckLib('pthread')
		conf.CheckPKGConfig('0.2')
		conf.CheckPKG('gl >= 7.0',1,0)
		conf.CheckPKG('sdl >= 1.2',1,0)
		conf.CheckPKG('SDL_image >= 1.2',1,0)
		conf.CheckPKG('libexif >= 0.6',0,'EXIF')
		conf.CheckPKG('ftgl >= 2.1',0,'FTGL')
		conf.CheckPKG('x11 >= 1.3',0,'X11')
		conf.CheckPKG('xext >= 1.1',0,'XEXT')

	conf.Define('APPNAME', env.subst('"slideshowgl"'))
	conf.Define('VERSION', env.subst('"2.0.0"'))

	env = conf.Finish()

env.Append(CCFLAGS = ['-Wall','-std=c99'])

if mode == 'debug':
	env.Append(CCFLAGS = ['-g'])
else:
	env.Append(CCFLAGS = ['-O2'])
#env.Append(CCFLAGS = ['-g'])
#env.Append(CCFLAGS = ['-O2'])

Export('env')
Export('prefix')

SConscript(build + '/src/SConscript')
SConscript(build + '/data/SConscript')

Import('install_bin')
Import('install_data')

env.Alias('install', [install_bin, install_data])
