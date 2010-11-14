env = Environment(APPNAME = 'slideshowgl', VERSION = '0.5.0')

mode = ARGUMENTS.get('mode')
os   = ARGUMENTS.get('os')

def CheckPKGConfig(context, version):
	context.Message( 'Checking for pkg-config... ' )
	ret = context.TryAction('pkg-config --atleast-pkgconfig-version=%s' % version)[0]
	context.Result( ret )
	return ret

def CheckPKG(context, name):
	context.Message( 'Checking for %s... ' % name )
	ret = context.TryAction('pkg-config --exists \'%s\'' % name)[0]
	context.Result( ret )
	return ret

build = 'build'
if os == 'win':
	build = 'build-win'

env.VariantDir(build, '.')

if not env.GetOption('clean'):

	conf = Configure(env, custom_tests = { 'CheckPKGConfig' : CheckPKGConfig, 'CheckPKG' : CheckPKG })
#	conf.CheckCCompiler()
	conf.CheckPKGConfig('0.2')
	conf.CheckLib('m')
	conf.CheckPKG('gl >= 7.0')
	conf.CheckPKG('sdl >= 1.2')
	conf.CheckPKG('SDL_image >= 1.2')

	conf.Define('APPNAME', env.subst('"slideshowgl"'))
	conf.Define('VERSION', env.subst('"2.0.0"'))

	env = conf.Finish()

if os == 'win':
	env.Replace(CC     = ['~/bin.mingw/gcc'])
	env.Append(CPPPATH = ['#/winlib/pthreads/Pre-built.2/include'])
	env.Append(CPPPATH = ['#/winlib/sdl/SDL-1.2.14/include/SDL/'])
	env.Append(CPPPATH = ['#/winlib/sdl_image/SDL_image-1.2.10/'])
	env.Append(LIBPATH = ['#/winlib/pthreads/Pre-built.2/lib'])
	env.Append(LIBPATH = ['#/winlib/sdl/SDL-1.2.14/lib/'])
	env.Append(LIBPATH = ['#/winlib/sdl_image/lib/'])
	env.Append(LIBS    = ['mingw32'])
	env.Append(LIBS    = ['SDLmain'])
	env.Append(LIBS    = ['SDL'])
	env.Append(LIBS    = ['SDL_image'])
	env.Append(LIBS    = ['opengl32'])
	env.Append(LIBS    = ['pthreadGCE2'])
else:
	env.ParseConfig('pkg-config --cflags --libs gl')
	env.ParseConfig('pkg-config --cflags --libs sdl')
	env.ParseConfig('pkg-config --cflags --libs SDL_image')

env.Append(CCFLAGS = ['-Wall','-std=c99'])

if mode == 'debug':
	env.Append(CCFLAGS = ['-g'])
else:
	env.Append(CCFLAGS = ['-O2'])
#env.Append(CCFLAGS = ['-g'])
#env.Append(CCFLAGS = ['-O2'])

Export('env')

SConscript(build + '/src/SConscript')

