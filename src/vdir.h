#ifndef _VDIR_H
#define _VDIR_H

#include <time.h>
#include "config.h"
#include "server.h"

struct cconnection;
struct cweb_server;

typedef int (*vdir_handler)( struct cconnection* conn ) ;

typedef struct cvdir{
	char name[PATH_LEN];
	vdir_handler exec;
}vdir;

int vdir_create( struct cweb_server* srv, char* name, vdir_handler handler );
int vdir_delete( struct cweb_server* srv, char* name );
int vdir_exec( struct cweb_server* srv, char* name, struct cconnection* conn );
void vdir_cleanup( struct cweb_server* srv );
int vdir_init ( struct cweb_server* srv );

#endif

