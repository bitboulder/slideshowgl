env = Environment(APPNAME = 'slideshowgl', VERSION = '0.5.0')

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


if not env.GetOption('clean'):
	env.VariantDir('build', '.')

	conf = Configure(env, custom_tests = { 'CheckPKGConfig' : CheckPKGConfig, 'CheckPKG' : CheckPKG })
#	conf.CheckCCompiler()
	conf.CheckPKGConfig('0.2')
	conf.CheckLib('m')
	conf.CheckPKG('sdl >= 1.2')
	conf.CheckPKG('gl >= 7.0')

	conf.Define('APPNAME', env.subst('"slideshowgl"'))
	conf.Define('VERSION', env.subst('"2.0.0"'))

	env = conf.Finish()

env.ParseConfig('pkg-config --cflags --libs sdl')
env.ParseConfig('pkg-config --cflags --libs gl')

env.Append(CCFLAGS = ['-Wall','-std=c99'])

#if env.DebugVariant():
#	env.Append(CCFLAGS = ['-g'])
#else:
#	env.Append(CCFLAGS = ['-O2'])
env.Append(CCFLAGS = ['-g'])

Export('env')

SConscript('build/src/SConscript')

