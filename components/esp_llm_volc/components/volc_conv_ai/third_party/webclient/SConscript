from building import *

cwd  = GetCurrentDir()
path = [cwd + '/inc']

src  = Glob('src/webclient.c')

if GetDepend(['WEBCLIENT_USING_FILE_DOWMLOAD']):
    src += Glob('src/webclient_file.c')

if GetDepend(['WEBCLIENT_USING_SAMPLES']):
    src += Glob('samples/*.c')

group = DefineGroup('WebClient', src, depend = ['PKG_USING_WEBCLIENT'], CPPPATH = path)

Return('group')
