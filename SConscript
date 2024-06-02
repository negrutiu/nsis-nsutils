target = 'NSutils'

files = Split("""
	gdi.c
	handles.c
	main.c
	registry.c
	strblock.c
	utils.c
	verinfo.c
""")

resources = Split("""
	NSutils.rc
""")

libs = Split("""
	kernel32
	user32
	advapi32
	gdi32
	ole32
	oleaut32
	uuid
	version
""")

examples = Split("""
	test/NSutils-Test.nsi
	test/NSutils-Test-build.bat
""")

docs = Split("""
	NSutils.Readme.txt
""")

Import('BuildPlugin')

# NOTE: We'll set cppused = True to link to standard libs (msvcrt.dll)
BuildPlugin(target, files, libs, examples, docs, res = resources, cppused = True)