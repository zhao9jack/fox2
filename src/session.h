#ifndef _SESSION_H
#define _SESSION_H

#include <time.h>
#include "config.h"

#define SESSIONKEY_LEN 16
struct cweb_server;
struct cclient;
typedef struct csession{
	struct csession*	next;
	struct csession*	pre;
	struct cweb_server *	server;
	struct cclient *	client;
	time_t 			time_create;
	time_t 			time_alive;
	int			reference;
	char			key[SESSIONKEY_LEN+1];
	void* 			data;
}session;

session* session_create( struct cclient* c );
session* session_get( struct cclient* c, char* key );

#endif
