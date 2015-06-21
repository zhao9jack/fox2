/*
 *  session.c
 *
 *  Session
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-7-10 13:31:53 Created.
 *
 *  Description: This file mainly includes the functions about 
 *  Session
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <pthread.h>
#include "debug.h"
#include "memory.h"
#include "server.h"
#include "client.h"
#include "session.h"


static void get_random_key( char* key )
{
	static uint seed = 1;
	int i;
	srand( time(NULL)+seed );
	for( i=0; i<SESSIONKEY_LEN; i++ ){
		key[i] = rand()%26+'A';
	}
	seed ++;
}

session* session_create( struct cclient* c )
{
	session* s;
	if( c->session_num >= c->server->max_onlines )
	{
		DBG("failed c->session_num: %d", c->session_num );
		return NULL;
	}
	NEW( s, sizeof(session) );
	memset( s, 0, sizeof(session) );
	LINK_APPEND( s, c->first_session );
	c->session_num ++;
	s->server = c->server;
	s->client = c;
	s->time_create = s->time_alive = time( NULL );
	//random a session key
	get_random_key( s->key );
	s->key[SESSIONKEY_LEN]=0;
	s->reference = 1;
	DBG("Create session %.16s", s->key );
	return s;
}

session* session_get( struct cclient* c, char* key )
{
	pthread_mutex_lock( &c->mutex_conn );
	pthread_mutex_lock( &c->mutex_session );
	session* s = NULL;
	if( *key ){
		for( s=c->first_session; s; s=s->next ){
			if( strncmp( s->key, key, SESSIONKEY_LEN ) == 0 ){
				s->reference ++;
				s->time_alive = time(NULL);
			//	DBG("Found Session: %s", key );
				break;
			}else{
			//	DBG("cmp failed: %.16s != %.16s", s->key, key );
			}
		}
	}
	if( s == NULL )
		s = session_create( c );
	pthread_mutex_unlock( &c->mutex_session );
	pthread_mutex_unlock( &c->mutex_conn );
	return s;
}
