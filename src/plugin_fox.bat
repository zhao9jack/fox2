gcc plugin_fox.c util.c loop.c xmlparser.c -o plugins/plugin_fox.dll -shared -Wl,--add-stdcall-alias -s -L. -lwsock32 -lpthreadgc2
