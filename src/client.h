#ifndef _CLIENT_H
#define _CLIENT_H

#include <pthread.h>
#include "config.h"
#include "connection.h"
#include "session.h"

#define LINK_APPEND( i, l ) { \
	i->next = l;	i->pre = NULL;	if(l) l->pre = i;	l=i;	}
#define LINK_DELETE( i, l ) { \
	if( i->pre ){ i->pre->next = i->next; }else{ l = i->next; }	\
	if( i->next ){ i->next->pre = i->pre; } \
	}

struct cweb_server;
typedef struct cclient{
	struct cclient *	pre;
	struct cclient *	next;
	struct cweb_server *	server;
	uint 			ip;
	char			ip_string[32];
	time_t 			time_create;
	
	//为了防止死锁，最好先锁定connection再锁定session
	//connection
	int 			conn_num;
	connection *		first_conn;
	pthread_mutex_t		mutex_conn;
	//session
	int 			session_num;
	session *		first_session;
	pthread_mutex_t		mutex_session;
	time_t			time_alive;
}client;

client* client_create( struct cweb_server* server, uint ip );
int client_end( client* c );
void client_print( client* c );
int client_live( client* c );

#endif
