gcc plugin_fcgi.c xmlparser.c loop.c -o plugins/plugin_fcgi.dll -shared -Wl,--add-stdcall-alias -L. -lwsock32 -lpthreadgc2
