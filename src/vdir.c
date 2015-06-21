/*
 *  vdir.c
 *
 *  Virtual Directory
 *
 *  Copyright (C) 2008  Huang Guan
 *
 *  2008-8-29 13:51:15 Created.
 *
 *  Description: This file mainly includes the functions about 
 *  Virtual Directory
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "debug.h"
#include "memory.h"
#include "server.h"
#include "connection.h"
#include "loop.h"
#include "vdir.h"

static void vdir_delete_fun( const void* p )
{
	DEL( p );
}

int vdir_init ( webserver* srv )
{
	loop_create( &srv->loop_vdir, MAX_VDIR, vdir_delete_fun );
	srv->vdir_create = vdir_create;
	srv->vdir_delete = vdir_delete;
	return 0;
}

int vdir_create( webserver* srv, char* name, vdir_handler handler )
{
	vdir * v;
	if( loop_is_full( &srv->loop_vdir ) ){
		DBG("vdir add failed: loop is full");
		return -1;
	}
	NEW( v, sizeof(vdir) );
	strncpy( v->name, name, PATH_LEN-1 );
	v->exec = handler ;
	loop_push_to_tail( &srv->loop_vdir, v );
	return 0;
}

static int vdir_searcher( const void* p, const void* v )
{
	return ( stricmp( ((vdir*)p)->name, v ) == 0 );
}

int vdir_delete( webserver* srv, char* name )
{
	vdir * v;
	v = loop_search( &srv->loop_vdir, name, vdir_searcher );
	if( v ){
		loop_remove( &srv->loop_vdir, v );
	}
	return 0;
}

int vdir_exec( webserver* srv, char* name, struct cconnection* conn )
{
	vdir * v;
	loop* l = &srv->loop_vdir;
	int i;
	for( i=l->head; i!=l->tail; i=(i+1)%l->size )
	{
		v = (vdir*)l->items[i];
		if( strncmp( conn->current_dir, v->name, strlen(v->name) ) == 0 ){
			if( v->exec( conn ) == 0 )
				return 1;
		}
	}
	return 0;
}


void vdir_cleanup( webserver* srv )
{
	loop_cleanup( &srv->loop_vdir );
}
