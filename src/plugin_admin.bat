gcc plugin_admin.c util.c -o plugins/plugin_admin.dll -lpthread -shared -Wl,--add-stdcall-alias -s -L. -lwsock32
