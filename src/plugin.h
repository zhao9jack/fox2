#ifndef _PLUGIN_H
#define _PLUGIN_H

#ifdef __WIN32__
#include <windows.h>
#endif

#include <time.h>
#include "config.h"
#include "server.h"


struct connection;
struct cweb_server;

typedef int (*plugin_entry)( struct cweb_server* srv ) ;

typedef struct cplugin{
	char name[PATH_LEN];
#ifdef __WIN32
	HMODULE handle;
#else
	uint handle;
#endif
	plugin_entry entry;
	plugin_entry cleanup;
	struct cweb_server* server;
}plugin;

int plugin_delete( struct cweb_server* srv, char* name );
void plugin_cleanup( struct cweb_server* srv );
int plugin_init ( struct cweb_server* srv );
int plugin_create( webserver* srv, char* name );

#endif
